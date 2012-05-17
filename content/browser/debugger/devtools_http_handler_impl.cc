// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_http_handler_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "grit/devtools_resources_map.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

const int kBufferSize = 16 * 1024;

namespace {

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
      net::HttpServer* server,
      int connection_id)
      : server_(server),
        connection_id_(connection_id) {
  }
  ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedContentsClosing() {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&net::HttpServer::Close, server_, connection_id_));
  }

  virtual void DispatchOnInspectorFrontend(const std::string& data) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   data));
  }

  virtual void ContentsReplaced(WebContents* new_contents) {
  }

 private:
  virtual void FrameNavigating(const std::string& url) {}
  net::HttpServer* server_;
  int connection_id_;
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
    const std::string& ip,
    int port,
    const std::string& frontend_url,
    net::URLRequestContextGetter* request_context_getter,
    DevToolsHttpHandlerDelegate* delegate) {
  DevToolsHttpHandlerImpl* http_handler =
      new DevToolsHttpHandlerImpl(ip,
                                  port,
                                  frontend_url,
                                  request_context_getter,
                                  delegate);
  http_handler->Start();
  return http_handler;
}

DevToolsHttpHandlerImpl::~DevToolsHttpHandlerImpl() {
  // Stop() must be called prior to this being called
  DCHECK(server_.get() == NULL);
}

void DevToolsHttpHandlerImpl::Start() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Init, this));
}

void DevToolsHttpHandlerImpl::Stop() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::TeardownAndRelease, this));
}

void DevToolsHttpHandlerImpl::SetRenderViewHostBinding(
    RenderViewHostBinding* binding) {
  if (binding)
    binding_ = binding;
  else
    binding_ = default_binding_.get();
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

void DevToolsHttpHandlerImpl::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
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

  if (info.path == "" || info.path == "/") {
    std::string response = delegate_->GetDiscoveryPageHTML();
    server_->Send200(connection_id, response, "text/html; charset=UTF-8");
    return;
  }

  // Proxy static files from chrome-devtools://devtools/*.
  net::URLRequestContext* request_context =
      request_context_getter_->GetURLRequestContext();
  if (!request_context) {
    server_->Send404(connection_id);
    return;
  }

  net::URLRequest* request;

  if (info.path.find("/devtools/") == 0) {
    // Serve front-end files from resource bundle.
    std::string filename = PathWithoutParams(info.path.substr(10));

    if (delegate_->BundlesFrontendResources()) {
      int resource_id = DevToolsHttpHandler::GetFrontendResourceId(filename);
      if (resource_id != -1) {
        base::StringPiece data =
            content::GetContentClient()->GetDataResource(resource_id);
        server_->Send200(connection_id,
                         data.as_string(),
                         GetMimeType(filename));
      }
      return;
    }
    std::string base_url = delegate_->GetFrontendResourcesBaseURL();
    request = new net::URLRequest(GURL(base_url + filename), this);
  } else if (info.path.find("/thumb/") == 0) {
    request = new net::URLRequest(GURL("chrome:/" + info.path), this);
  } else {
    server_->Send404(connection_id);
    return;
  }

  Bind(request, connection_id);
  request->set_context(request_context);
  request->Start();
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
  ConnectionToRequestsMap::iterator it =
      connection_to_requests_io_.find(connection_id);
  if (it != connection_to_requests_io_.end()) {
    // Dispose delegating socket.
    for (std::set<net::URLRequest*>::iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      net::URLRequest* request = *it2;
      request->Cancel();
      request_to_connection_io_.erase(request);
      request_to_buffer_io_.erase(request);
      delete request;
    }
    connection_to_requests_io_.erase(connection_id);
  }

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
      content::RenderViewHostDelegate* host_delegate = host->GetDelegate();

      DevToolsAgentHost* agent =
          DevToolsAgentHostRegistry::GetDevToolsAgentHost(host);
      DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
          GetDevToolsClientHostFor(agent);
      PageInfo page_info;
      page_info.id = binding_->GetIdentifier(host);
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
      page_list.push_back(page_info);
    }
  }
  std::sort(page_list.begin(), page_list.end(), SortPageListByTime);
  return page_list;
}

void DevToolsHttpHandlerImpl::OnJsonRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  PageList page_list = GeneratePageList();
  ListValue json_pages_list;
  std::string host = info.headers["Host"];
  for (PageList::iterator i = page_list.begin();
       i != page_list.end(); ++i) {
    DictionaryValue* page_info = new DictionaryValue;
    json_pages_list.Append(page_info);
    page_info->SetString("title", i->title);
    page_info->SetString("url", i->url);
    page_info->SetString("thumbnailUrl", i->thumbnail_url);
    page_info->SetString("faviconUrl", i->favicon_url);
    if (!i->attached) {
      page_info->SetString("webSocketDebuggerUrl",
                           base::StringPrintf("ws://%s/devtools/page/%s",
                                              host.c_str(),
                                              i->id.c_str()));
      std::string devtools_frontend_url = base::StringPrintf(
          "%s%sws=%s/devtools/page/%s",
          overridden_frontend_url_.c_str(),
          overridden_frontend_url_.find("?") == std::string::npos ? "?" : "&",
          host.c_str(),
          i->id.c_str());
      page_info->SetString("devtoolsFrontendUrl", devtools_frontend_url);
    }
  }

  std::string response;
  base::JSONWriter::WriteWithOptions(&json_pages_list,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &response);
  Send200(connection_id, response, "application/json; charset=UTF-8");
}

void DevToolsHttpHandlerImpl::OnWebSocketRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
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
      new DevToolsClientHostImpl(server_, connection_id);
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

void DevToolsHttpHandlerImpl::OnResponseStarted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;

  std::string content_type;
  request->GetMimeType(&content_type);

  if (request->status().is_success()) {
    server_->Send(connection_id,
                  base::StringPrintf("HTTP/1.1 200 OK\r\n"
                                     "Content-Type:%s\r\n"
                                     "Transfer-Encoding: chunked\r\n"
                                     "\r\n",
                                     content_type.c_str()));
  } else {
    server_->Send404(connection_id);
  }

  int bytes_read = 0;
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  if (request->status().is_success())
    request->Read(buffer, kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void DevToolsHttpHandlerImpl::OnReadCompleted(net::URLRequest* request,
                                                  int bytes_read) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;

  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  do {
    if (!request->status().is_success() || bytes_read <= 0)
      break;
    std::string chunk_size = base::StringPrintf("%X\r\n", bytes_read);
    server_->Send(connection_id, chunk_size);
    server_->Send(connection_id, buffer->data(), bytes_read);
    server_->Send(connection_id, "\r\n");
  } while (request->Read(buffer, kBufferSize, &bytes_read));


  // See comments re: HEAD requests in OnResponseStarted().
  if (!request->status().is_io_pending()) {
    server_->Send(connection_id, "0\r\n\r\n");
    RequestCompleted(request);
  }
}

DevToolsHttpHandlerImpl::DevToolsHttpHandlerImpl(
    const std::string& ip,
    int port,
    const std::string& frontend_url,
    net::URLRequestContextGetter* request_context_getter,
    DevToolsHttpHandlerDelegate* delegate)
    : ip_(ip),
      port_(port),
      overridden_frontend_url_(frontend_url),
      request_context_getter_(request_context_getter),
      delegate_(delegate) {
  if (overridden_frontend_url_.empty())
      overridden_frontend_url_ = "/devtools/devtools.html";

  default_binding_.reset(new DevToolsDefaultBindingHandler);
  binding_ = default_binding_.get();

  AddRef();
}

void DevToolsHttpHandlerImpl::Init() {
  server_ = new net::HttpServer(ip_, port_, this);
}

// Run on I/O thread
void DevToolsHttpHandlerImpl::TeardownAndRelease() {
  server_ = NULL;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Release, this));
}

void DevToolsHttpHandlerImpl::Bind(net::URLRequest* request,
                                       int connection_id) {
  request_to_connection_io_[request] = connection_id;
  ConnectionToRequestsMap::iterator it =
      connection_to_requests_io_.find(connection_id);
  if (it == connection_to_requests_io_.end()) {
    std::pair<int, std::set<net::URLRequest*> > value(
        connection_id,
        std::set<net::URLRequest*>());
    it = connection_to_requests_io_.insert(value).first;
  }
  it->second.insert(request);
  request_to_buffer_io_[request] = new net::IOBuffer(kBufferSize);
}

void DevToolsHttpHandlerImpl::RequestCompleted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;
  request_to_connection_io_.erase(request);
  ConnectionToRequestsMap::iterator it2 =
      connection_to_requests_io_.find(connection_id);
  it2->second.erase(request);
  request_to_buffer_io_.erase(request);
  delete request;
}

void DevToolsHttpHandlerImpl::Send200(int connection_id,
                                          const std::string& data,
                                          const std::string& mime_type) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&net::HttpServer::Send200,
                 server_.get(),
                 connection_id,
                 data,
                 mime_type));
}

void DevToolsHttpHandlerImpl::Send404(int connection_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&net::HttpServer::Send404, server_.get(), connection_id));
}

void DevToolsHttpHandlerImpl::Send500(int connection_id,
                                      const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&net::HttpServer::Send500, server_.get(), connection_id,
                 message));
}

void DevToolsHttpHandlerImpl::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&net::HttpServer::AcceptWebSocket, server_.get(),
                 connection_id, request));
}

}  // namespace content
