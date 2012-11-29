// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_http_handler_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/devtools_resources_map.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server_request_info.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "ui/base/layout.h"
#include "webkit/user_agent/user_agent.h"
#include "webkit/user_agent/user_agent_util.h"

namespace content {

const int kBufferSize = 16 * 1024;

namespace {

static const char* kDevToolsHandlerThreadName = "Chrome_DevToolsHandlerThread";

class DevToolsDefaultBindingHandler
    : public DevToolsHttpHandler::RenderViewHostBinding {
 public:
  DevToolsDefaultBindingHandler() {
  }

  virtual std::string GetIdentifier(RenderViewHost* rvh) OVERRIDE {
    int process_id = rvh->GetProcess()->GetID();
    int routing_id = rvh->GetRoutingID();
    return base::StringPrintf("%d_%d", process_id, routing_id);
  }

  virtual RenderViewHost* ForIdentifier(
      const std::string& identifier) OVERRIDE {
    size_t pos = identifier.find("_");
    if (pos == std::string::npos)
      return NULL;

    int process_id;
    if (!base::StringToInt(identifier.substr(0, pos), &process_id))
      return NULL;

    int routing_id;
    if (!base::StringToInt(identifier.substr(pos+1), &routing_id))
      return NULL;

    return RenderViewHost::FromID(process_id, routing_id);
  }
};


// An internal implementation of DevToolsClientHost that delegates
// messages sent for DevToolsClient to a DebuggerShell instance.
class DevToolsClientHostImpl : public DevToolsClientHost {
 public:
  DevToolsClientHostImpl(
      MessageLoop* message_loop,
      net::HttpServer* server,
      int connection_id)
      : message_loop_(message_loop),
        server_(server),
        connection_id_(connection_id),
        is_closed_(false),
        detach_reason_("target_closed") {
  }

  ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedContentsClosing() {
    if (is_closed_)
      return;
    is_closed_ = true;

    std::string response =
        WebKit::WebDevToolsAgent::inspectorDetachedEvent(
            WebKit::WebString::fromUTF8(detach_reason_)).utf8();
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   response));

    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::Close, server_, connection_id_));
  }

  virtual void DispatchOnInspectorFrontend(const std::string& data) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   data));
  }

  virtual void ContentsReplaced(WebContents* new_contents) {
  }

  virtual void ReplacedWithAnotherClient() {
    detach_reason_ = "replaced_with_devtools";
  }

 private:
  virtual void FrameNavigating(const std::string& url) {}
  MessageLoop* message_loop_;
  net::HttpServer* server_;
  int connection_id_;
  bool is_closed_;
  std::string detach_reason_;
};

}  // namespace

// static
int DevToolsHttpHandler::GetFrontendResourceId(const std::string& name) {
  for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
    if (name == kDevtoolsResources[i].name)
      return kDevtoolsResources[i].value;
  }
  return -1;
}

// static
DevToolsHttpHandler* DevToolsHttpHandler::Start(
    const net::StreamListenSocketFactory* socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate) {
  DevToolsHttpHandlerImpl* http_handler =
      new DevToolsHttpHandlerImpl(socket_factory,
                                  frontend_url,
                                  delegate);
  http_handler->Start();
  return http_handler;
}

DevToolsHttpHandlerImpl::~DevToolsHttpHandlerImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Stop() must be called prior to destruction.
  DCHECK(server_.get() == NULL);
  DCHECK(thread_.get() == NULL);
}

void DevToolsHttpHandlerImpl::Start() {
  if (thread_.get())
    return;
  thread_.reset(new base::Thread(kDevToolsHandlerThreadName));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::StartHandlerThread, this));
}

// Runs on FILE thread.
void DevToolsHttpHandlerImpl::StartHandlerThread() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::ResetHandlerThread, this));
    return;
  }

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Init, this));
}

void DevToolsHttpHandlerImpl::ResetHandlerThread() {
  thread_.reset();
}

void DevToolsHttpHandlerImpl::ResetHandlerThreadAndRelease() {
  ResetHandlerThread();
  Release();
}

void DevToolsHttpHandlerImpl::Stop() {
  if (!thread_.get())
    return;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::StopHandlerThread, this),
      base::Bind(&DevToolsHttpHandlerImpl::ResetHandlerThreadAndRelease, this));
}

void DevToolsHttpHandlerImpl::SetRenderViewHostBinding(
    RenderViewHostBinding* binding) {
  if (binding)
    binding_ = binding;
  else
    binding_ = default_binding_.get();
}

GURL DevToolsHttpHandlerImpl::GetFrontendURL(RenderViewHost* render_view_host) {
  net::IPEndPoint ip_address;
  if (server_->GetLocalAddress(&ip_address))
    return GURL();
  std::string host = ip_address.ToString();
  std::string id = binding_->GetIdentifier(render_view_host);
  return GURL(std::string("http://") +
              ip_address.ToString() +
              GetFrontendURLInternal(id, host));
}

static std::string PathWithoutParams(const std::string& path) {
  size_t query_position = path.find("?");
  if (query_position != std::string::npos)
    return path.substr(0, query_position);
  return path;
}

static std::string GetMimeType(const std::string& filename) {
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  }
  NOTREACHED();
  return "text/plain";
}

void DevToolsHttpHandlerImpl::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();
  DevToolsManager* manager = DevToolsManager::GetInstance();
  for (ConnectionToClientHostMap::iterator it =
       connection_to_client_host_ui_.begin();
       it != connection_to_client_host_ui_.end(); ++it) {
    DevToolsAgentHost* agent = manager->GetDevToolsAgentHostFor(it->second);
    if (!agent)
      continue;
    RenderViewHost* rvh = DevToolsAgentHostRegistry::GetRenderViewHost(agent);
    if (rvh && rvh->GetProcess() == process)
      it->second->InspectedContentsClosing();
  }
}

void DevToolsHttpHandlerImpl::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  if (info.path.find("/json/version") == 0) {
    // New page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnVersionRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find("/json/new") == 0) {
    // New page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnNewTargetRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find("/json/close/") == 0) {
    // Close page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnCloseTargetRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find("/json") == 0) {
    // Pages discovery json request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnJsonRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find("/thumb/") == 0) {
    // Thumbnail request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnThumbnailRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path == "" || info.path == "/") {
    // Discovery page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnDiscoveryPageRequestUI,
                   this,
                   connection_id));
    return;
  }

  if (info.path.find("/devtools/") != 0) {
    server_->Send404(connection_id);
    return;
  }

  std::string filename = PathWithoutParams(info.path.substr(10));
  std::string mime_type = GetMimeType(filename);

  FilePath frontend_dir = delegate_->GetDebugFrontendDir();
  if (!frontend_dir.empty()) {
    FilePath path = frontend_dir.AppendASCII(filename);
    std::string data;
    file_util::ReadFileToString(path, &data);
    server_->Send200(connection_id, data, mime_type);
    return;
  }
  if (delegate_->BundlesFrontendResources()) {
    int resource_id = DevToolsHttpHandler::GetFrontendResourceId(filename);
    if (resource_id != -1) {
      base::StringPiece data = GetContentClient()->GetDataResource(
              resource_id, ui::SCALE_FACTOR_NONE);
      server_->Send200(connection_id, data.as_string(), mime_type);
      return;
    }
  }
  server_->Send404(connection_id);
}

void DevToolsHttpHandlerImpl::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketRequestUI,
          this,
          connection_id,
          request));
}

void DevToolsHttpHandlerImpl::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketMessageUI,
          this,
          connection_id,
          data));
}

void DevToolsHttpHandlerImpl::OnClose(int connection_id) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnCloseUI,
          this,
          connection_id));
}

struct DevToolsHttpHandlerImpl::PageInfo {
  PageInfo()
      : attached(false) {
  }

  std::string id;
  std::string url;
  bool attached;
  std::string title;
  std::string thumbnail_url;
  std::string favicon_url;
  base::TimeTicks last_selected_time;
};

// static
bool DevToolsHttpHandlerImpl::SortPageListByTime(const PageInfo& info1,
                                                 const PageInfo& info2) {
  return info1.last_selected_time > info2.last_selected_time;
}

DevToolsHttpHandlerImpl::PageList DevToolsHttpHandlerImpl::GeneratePageList() {
  PageList page_list;
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* render_process_host = it.GetCurrentValue();
    DCHECK(render_process_host);

    // Ignore processes that don't have a connection, such as crashed contents.
    if (!render_process_host->HasConnection())
      continue;

    RenderProcessHost::RenderWidgetHostsIterator rwit(
        render_process_host->GetRenderWidgetHostsIterator());
    for (; !rwit.IsAtEnd(); rwit.Advance()) {
      const RenderWidgetHost* widget = rwit.GetCurrentValue();
      DCHECK(widget);
      if (!widget || !widget->IsRenderView())
        continue;

      RenderViewHost* host =
          RenderViewHost::From(const_cast<RenderWidgetHost*>(widget));
      page_list.push_back(CreatePageInfo(host));
    }
  }
  std::sort(page_list.begin(), page_list.end(), SortPageListByTime);
  return page_list;
}

std::string DevToolsHttpHandlerImpl::GetFrontendURLInternal(
    const std::string rvh_id,
    const std::string& host) {
  return base::StringPrintf(
      "%s%sws=%s/devtools/page/%s",
      overridden_frontend_url_.c_str(),
      overridden_frontend_url_.find("?") == std::string::npos ? "?" : "&",
      host.c_str(),
      rvh_id.c_str());
}

void DevToolsHttpHandlerImpl::OnVersionRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  DictionaryValue version;
  version.SetString("Protocol-Version",
                    WebKit::WebDevToolsAgent::inspectorProtocolVersion());
  version.SetString("WebKit-Version",
                    webkit_glue::GetWebKitVersion());
  version.SetString("User-Agent",
                    webkit_glue::GetUserAgent(GURL(chrome::kAboutBlankURL)));
  SendJson(connection_id, info, version);
}

void DevToolsHttpHandlerImpl::OnJsonRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  PageList page_list = GeneratePageList();
  ListValue json_pages_list;
  std::string host = info.headers["Host"];
  for (PageList::iterator i = page_list.begin(); i != page_list.end(); ++i)
    json_pages_list.Append(SerializePageInfo(*i, host));
  SendJson(connection_id, info, json_pages_list);
}

void DevToolsHttpHandlerImpl::OnNewTargetRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  RenderViewHost* rvh = delegate_->CreateNewTarget();
  if (!rvh) {
    Send500(connection_id, "Could not create new page");
    return;
  }
  PageInfo page_info = CreatePageInfo(rvh);
  std::string host = info.headers["Host"];
  scoped_ptr<DictionaryValue> dictionary(SerializePageInfo(page_info, host));
  SendJson(connection_id, info, *dictionary);
}

void DevToolsHttpHandlerImpl::OnCloseTargetRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  std::string prefix = "/json/close/";
  std::string page_id = info.path.substr(prefix.length());
  RenderViewHost* rvh = binding_->ForIdentifier(page_id);
  if (!rvh) {
    Send500(connection_id, "No such target id: " + page_id);
    return;
  }

  rvh->ClosePage();
  Send200(connection_id, "Target is closing");
}

void DevToolsHttpHandlerImpl::OnThumbnailRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  std::string prefix = "/thumb/";
  size_t pos = info.path.find(prefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_url = info.path.substr(prefix.length());
  std::string data = delegate_->GetPageThumbnailData(GURL(page_url));
  if (!data.empty())
    Send200(connection_id, data, "image/png");
  else
    Send404(connection_id);
}

void DevToolsHttpHandlerImpl::OnDiscoveryPageRequestUI(int connection_id) {
  std::string response = delegate_->GetDiscoveryPageHTML();
  Send200(connection_id, response, "text/html; charset=UTF-8");
}

void DevToolsHttpHandlerImpl::OnWebSocketRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_.get())
    return;

  std::string prefix = "/devtools/page/";
  size_t pos = request.path.find(prefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_id = request.path.substr(prefix.length());
  RenderViewHost* rvh = binding_->ForIdentifier(page_id);
  if (!rvh) {
    Send500(connection_id, "No such target id: " + page_id);
    return;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      rvh);
  if (manager->GetDevToolsClientHostFor(agent)) {
    Send500(connection_id, "Target with given id is being inspected: " +
        page_id);
    return;
  }

  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(thread_->message_loop(),
                                 server_,
                                 connection_id);
  connection_to_client_host_ui_[connection_id] = client_host;

  manager->RegisterDevToolsClientHostFor(agent, client_host);

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpHandlerImpl::OnWebSocketMessageUI(
    int connection_id,
    const std::string& data) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it == connection_to_client_host_ui_.end())
    return;

  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->DispatchOnInspectorBackend(it->second, data);
}

void DevToolsHttpHandlerImpl::OnCloseUI(int connection_id) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it != connection_to_client_host_ui_.end()) {
    DevToolsClientHostImpl* client_host =
        static_cast<DevToolsClientHostImpl*>(it->second);
    DevToolsManager::GetInstance()->ClientHostClosing(client_host);
    delete client_host;
    connection_to_client_host_ui_.erase(connection_id);
  }
}

DevToolsHttpHandlerImpl::DevToolsHttpHandlerImpl(
    const net::StreamListenSocketFactory* socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate)
    : overridden_frontend_url_(frontend_url),
      socket_factory_(socket_factory),
      delegate_(delegate) {
  if (overridden_frontend_url_.empty())
      overridden_frontend_url_ = "/devtools/devtools.html";

  default_binding_.reset(new DevToolsDefaultBindingHandler);
  binding_ = default_binding_.get();

  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());

  // Balanced in ResetHandlerThreadAndRelease().
  AddRef();
}

// Runs on the handler thread
void DevToolsHttpHandlerImpl::Init() {
  server_ = new net::HttpServer(*socket_factory_.get(), this);
}

// Runs on the handler thread
void DevToolsHttpHandlerImpl::Teardown() {
  server_ = NULL;
}

// Runs on FILE thread to make sure that it is serialized against
// {Start|Stop}HandlerThread and to allow calling pthread_join.
void DevToolsHttpHandlerImpl::StopHandlerThread() {
  if (!thread_->message_loop())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Teardown, this));
  // Thread::Stop joins the thread.
  thread_->Stop();
}

void DevToolsHttpHandlerImpl::Send200(int connection_id,
                                      const std::string& data,
                                      const std::string& mime_type) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send200,
                 server_.get(),
                 connection_id,
                 data,
                 mime_type));
}

void DevToolsHttpHandlerImpl::SendJson(int connection_id,
                                       const net::HttpServerRequestInfo& info,
                                       const Value& value) {
  std::string response;
  base::JSONWriter::WriteWithOptions(&value,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &response);

  size_t jsonp_pos = info.path.find("?jsonp=");
  if (jsonp_pos == std::string::npos) {
    Send200(connection_id, response, "application/json; charset=UTF-8");
    return;
  }

  std::string jsonp = info.path.substr(jsonp_pos + 7);
  response = StringPrintf("%s(%s);", jsonp.c_str(), response.c_str());
  Send200(connection_id, response, "text/javascript; charset=UTF-8");
}

void DevToolsHttpHandlerImpl::Send404(int connection_id) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send404, server_.get(), connection_id));
}

void DevToolsHttpHandlerImpl::Send500(int connection_id,
                                      const std::string& message) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send500, server_.get(), connection_id,
                 message));
}

void DevToolsHttpHandlerImpl::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_.get())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::AcceptWebSocket, server_.get(),
                 connection_id, request));
}

DevToolsHttpHandlerImpl::PageInfo
DevToolsHttpHandlerImpl::CreatePageInfo(RenderViewHost* rvh)
{
  RenderViewHostDelegate* host_delegate = rvh->GetDelegate();
  DevToolsAgentHost* agent =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh);
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(agent);
  PageInfo page_info;
  page_info.id = binding_->GetIdentifier(rvh);
  page_info.attached = client_host != NULL;
  page_info.url = host_delegate->GetURL().spec();

  WebContents* web_contents = host_delegate->GetAsWebContents();
  if (web_contents) {
    page_info.title = UTF16ToUTF8(
        net::EscapeForHTML(web_contents->GetTitle()));
    page_info.last_selected_time = web_contents->GetLastSelectedTime();

    NavigationController& controller = web_contents->GetController();
    NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid()) {
      page_info.thumbnail_url = "/thumb/" + entry->GetURL().spec();
      page_info.favicon_url = entry->GetFavicon().url.spec();
    }
  }
  return page_info;
}

DictionaryValue* DevToolsHttpHandlerImpl::SerializePageInfo(
    const PageInfo& page_info,
    const std::string& host) {
  DictionaryValue* dictionary = new DictionaryValue;
  dictionary->SetString("title", page_info.title);
  dictionary->SetString("url", page_info.url);
  dictionary->SetString("thumbnailUrl", page_info.thumbnail_url);
  dictionary->SetString("faviconUrl", page_info.favicon_url);
  if (!page_info.attached) {
    dictionary->SetString("webSocketDebuggerUrl",
                          base::StringPrintf("ws://%s/devtools/page/%s",
                                             host.c_str(),
                                             page_info.id.c_str()));
    std::string devtools_frontend_url = GetFrontendURLInternal(
        page_info.id.c_str(),
        host);
    dictionary->SetString("devtoolsFrontendUrl", devtools_frontend_url);
  }
  return dictionary;
}

}  // namespace content
