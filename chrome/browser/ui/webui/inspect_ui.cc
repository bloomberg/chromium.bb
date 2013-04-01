// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inspect_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol_http_request.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/worker_service.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/process_type.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::ChildProcessData;
using content::DevToolsAgentHost;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderViewHostDelegate;
using content::RenderWidgetHost;
using content::WebContents;
using content::WebUIMessageHandler;
using content::WorkerService;
using content::WorkerServiceObserver;

namespace {

static const char kDataFile[] = "targets-data.json";
static const char kAdbQuery[] = "adb-query/";
static const char kAdbPages[] = "adb-pages";
static const char kLocalXhr[] = "local-xhr/";

static const char kExtensionTargetType[]  = "extension";
static const char kPageTargetType[]  = "page";
static const char kWorkerTargetType[]  = "worker";
static const char kAdbTargetType[]  = "adb_page";

static const char kInspectCommand[]  = "inspect";
static const char kTerminateCommand[]  = "terminate";

static const char kTargetTypeField[]  = "type";
static const char kAttachedField[]  = "attached";
static const char kProcessIdField[]  = "processId";
static const char kRouteIdField[]  = "routeId";
static const char kUrlField[]  = "url";
static const char kNameField[]  = "name";
static const char kFaviconUrlField[] = "faviconUrl";
static const char kPidField[]  = "pid";
static const char kAdbSerialField[] = "adbSerial";
static const char kAdbModelField[] = "adbModel";
static const char kAdbPageIdField[] = "adbPageId";
static const char kAdbDebugUrl[] = "adbDebugUrl";

DictionaryValue* BuildTargetDescriptor(
    const std::string& target_type,
    bool attached,
    const GURL& url,
    const std::string& name,
    const GURL& favicon_url,
    int process_id,
    int route_id,
    base::ProcessHandle handle = base::kNullProcessHandle) {
  DictionaryValue* target_data = new DictionaryValue();
  target_data->SetString(kTargetTypeField, target_type);
  target_data->SetBoolean(kAttachedField, attached);
  target_data->SetInteger(kProcessIdField, process_id);
  target_data->SetInteger(kRouteIdField, route_id);
  target_data->SetString(kUrlField, url.spec());
  target_data->SetString(kNameField, net::EscapeForHTML(name));
  target_data->SetInteger(kPidField, base::GetProcId(handle));
  target_data->SetString(kFaviconUrlField, favicon_url.spec());

  return target_data;
}

bool HasClientHost(RenderViewHost* rvh) {
  if (!DevToolsAgentHost::HasFor(rvh))
    return false;

  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(rvh));
  return agent->IsAttached();
}

DictionaryValue* BuildTargetDescriptor(RenderViewHost* rvh, bool is_tab) {
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  std::string title;
  std::string target_type = is_tab ? kPageTargetType : "";
  GURL url;
  GURL favicon_url;
  if (web_contents) {
    url = web_contents->GetURL();
    title = UTF16ToUTF8(web_contents->GetTitle());
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url = entry->GetFavicon().url;

    Profile* profile = Profile::FromBrowserContext(
        web_contents->GetBrowserContext());
    if (profile) {
      ExtensionService* extension_service = profile->GetExtensionService();
      const extensions::Extension* extension = extension_service->
          extensions()->GetByID(url.host());
      if (extension) {
        target_type = kExtensionTargetType;
        title = extension->name();
      }
    }
  }

  return BuildTargetDescriptor(target_type,
                               HasClientHost(rvh),
                               url,
                               title,
                               favicon_url,
                               rvh->GetProcess()->GetID(),
                               rvh->GetRoutingID());
}

// Appends the inspectable workers to the list of RenderViews, and sends the
// response back to the webui system.
void SendDescriptors(
    ListValue* rvh_list,
    const content::WebUIDataSource::GotDataCallback& callback) {
  std::vector<WorkerService::WorkerInfo> worker_info =
      WorkerService::GetInstance()->GetWorkers();
  for (size_t i = 0; i < worker_info.size(); ++i) {
    rvh_list->Append(BuildTargetDescriptor(
        kWorkerTargetType,
        false,
        worker_info[i].url,
        UTF16ToUTF8(worker_info[i].name),
        GURL(),
        worker_info[i].process_id,
        worker_info[i].route_id,
        worker_info[i].handle));
  }

  std::string json_string;
  base::JSONWriter::Write(rvh_list, &json_string);
  callback.Run(base::RefCountedString::TakeString(&json_string));
}

bool HandleDataRequestCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  std::set<RenderViewHost*> tab_rvhs;
  for (TabContentsIterator it; !it.done(); it.Next())
    tab_rvhs.insert(it->GetRenderViewHost());

  scoped_ptr<ListValue> rvh_list(new ListValue());

  std::vector<RenderViewHost*> rvh_vector =
      DevToolsAgentHost::GetValidRenderViewHosts();

  for (std::vector<RenderViewHost*>::iterator it(rvh_vector.begin());
       it != rvh_vector.end(); it++) {
    bool is_tab = tab_rvhs.find(*it) != tab_rvhs.end();
    rvh_list->Append(BuildTargetDescriptor(*it, is_tab));
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SendDescriptors, base::Owned(rvh_list.release()), callback));
  return true;
}

class InspectMessageHandler : public WebUIMessageHandler {
 public:
  InspectMessageHandler() {}
  virtual ~InspectMessageHandler() {}

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for "openDevTools" message.
  void HandleInspectCommand(const ListValue* args);
  void HandleTerminateCommand(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(InspectMessageHandler);
};

void InspectMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kInspectCommand,
      base::Bind(&InspectMessageHandler::HandleInspectCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kTerminateCommand,
      base::Bind(&InspectMessageHandler::HandleTerminateCommand,
                 base::Unretained(this)));
}

void InspectMessageHandler::HandleInspectCommand(const ListValue* args) {
  std::string process_id_str;
  std::string route_id_str;
  int process_id;
  int route_id;
  CHECK(args->GetSize() == 2);
  CHECK(args->GetString(0, &process_id_str));
  CHECK(args->GetString(1, &route_id_str));
  CHECK(base::StringToInt(process_id_str,
                          &process_id));
  CHECK(base::StringToInt(route_id_str, &route_id));

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  RenderViewHost* rvh = RenderViewHost::FromID(process_id, route_id);
  if (rvh) {
    DevToolsWindow::OpenDevToolsWindow(rvh);
    return;
  }

  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForWorker(process_id, route_id));
  if (agent_host)
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
}

static void TerminateWorker(int process_id, int route_id) {
  WorkerService::GetInstance()->TerminateWorker(process_id, route_id);
}

void InspectMessageHandler::HandleTerminateCommand(const ListValue* args) {
  std::string process_id_str;
  std::string route_id_str;
  int process_id;
  int route_id;
  CHECK(args->GetSize() == 2);
  CHECK(args->GetString(0, &process_id_str));
  CHECK(args->GetString(1, &route_id_str));
  CHECK(base::StringToInt(process_id_str,
                          &process_id));
  CHECK(base::StringToInt(route_id_str, &route_id));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TerminateWorker, process_id, route_id));
}

}  // namespace

class InspectUI::WorkerCreationDestructionListener
    : public WorkerServiceObserver,
      public base::RefCountedThreadSafe<WorkerCreationDestructionListener> {
 public:
  explicit WorkerCreationDestructionListener(InspectUI* workers_ui)
      : discovery_ui_(workers_ui) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::RegisterObserver,
                   this));
  }

  void InspectUIDestroyed() {
    discovery_ui_ = NULL;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::UnregisterObserver,
                   this));
  }

 private:
  friend class base::RefCountedThreadSafe<WorkerCreationDestructionListener>;
  virtual ~WorkerCreationDestructionListener() {}

  virtual void WorkerCreated(
      const GURL& url,
      const string16& name,
      int process_id,
      int route_id) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::NotifyItemsChanged,
                   this));
  }

  virtual void WorkerDestroyed(int process_id, int route_id) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::NotifyItemsChanged,
                   this));
  }

  void NotifyItemsChanged() {
    if (discovery_ui_)
      discovery_ui_->RefreshUI();
  }

  void RegisterObserver() {
    WorkerService::GetInstance()->AddObserver(this);
  }

  void UnregisterObserver() {
    WorkerService::GetInstance()->RemoveObserver(this);
  }

  InspectUI* discovery_ui_;
};

InspectUI::InspectUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      observer_(new WorkerCreationDestructionListener(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  web_ui->AddMessageHandler(new InspectMessageHandler());

  Profile* profile = Profile::FromWebUI(web_ui);
  adb_bridge_.reset(new DevToolsAdbBridge(profile));
  content::WebUIDataSource::Add(profile, CreateInspectUIHTMLSource());

  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllSources());
}

InspectUI::~InspectUI() {
  StopListeningNotifications();
}

void InspectUI::RefreshUI() {
  web_ui()->CallJavascriptFunction("populateLists");
}

// static
bool InspectUI::WeakHandleRequestCallback(
    const base::WeakPtr<InspectUI>& inspect_ui,
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  if (!inspect_ui.get())
    return false;
  return inspect_ui->HandleRequestCallback(path, callback);
}

void InspectUI::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (source != content::Source<WebContents>(web_ui()->GetWebContents()))
    RefreshUI();
  else if (type == content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED)
    StopListeningNotifications();
}

void InspectUI::StopListeningNotifications()
{
  if (!observer_)
    return;
  adb_bridge_.reset();
  observer_->InspectUIDestroyed();
  observer_ = NULL;
  registrar_.RemoveAll();
}

content::WebUIDataSource* InspectUI::CreateInspectUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIInspectHost);
  source->AddResourcePath("inspect.css", IDR_INSPECT_CSS);
  source->AddResourcePath("inspect.js", IDR_INSPECT_JS);
  source->SetDefaultResource(IDR_INSPECT_HTML);
  source->SetRequestFilter(base::Bind(&InspectUI::WeakHandleRequestCallback,
                                      weak_factory_.GetWeakPtr()));
  return source;
}

bool InspectUI::HandleRequestCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  if (path == kDataFile)
    return HandleDataRequestCallback(path, callback);
  if (path.find(kAdbPages) == 0)
    return HandleAdbPagesCallback(path, callback);
  if (path.find(kAdbQuery) == 0)
    return HandleAdbQueryCallback(path, callback);
  if (path.find(kLocalXhr) == 0)
    return HandleLocalXhrCallback(path, callback);
  return false;
}

bool InspectUI::HandleAdbQueryCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  std::string query = net::UnescapeURLComponent(
      path.substr(strlen(kAdbQuery)),
      net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
          net::UnescapeRule::URL_SPECIAL_CHARS);
  adb_bridge_->Query(query, base::Bind(&InspectUI::RespondOnUIThread,
                                       weak_factory_.GetWeakPtr(), callback));
  return true;
}

bool InspectUI::HandleAdbPagesCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  adb_bridge_->Pages(base::Bind(&InspectUI::OnAdbPages,
                                weak_factory_.GetWeakPtr(),
                                callback));
  return true;
}

void InspectUI::OnAdbPages(
    const content::WebUIDataSource::GotDataCallback& callback,
    int result,
    DevToolsAdbBridge::RemotePages* pages) {
  if (result != net::OK)
    return;
  ListValue targets;
  scoped_ptr<DevToolsAdbBridge::RemotePages> my_pages(pages);
  for (DevToolsAdbBridge::RemotePages::iterator it = my_pages->begin();
       it != my_pages->end(); ++it) {
    DevToolsAdbBridge::RemotePage* page = it->get();
    DictionaryValue* target_data = BuildTargetDescriptor(kAdbTargetType,
        false, GURL(page->url()), page->title(), GURL(page->favicon_url()), 0,
        0);
    target_data->SetString(kAdbSerialField, page->serial());
    target_data->SetString(kAdbModelField, page->model());
    target_data->SetString(kAdbPageIdField, page->id());
    target_data->SetString(kAdbDebugUrl, page->debug_url());
    targets.Append(target_data);
  }

  std::string json_string = "";
  base::JSONWriter::Write(&targets, &json_string);
  callback.Run(base::RefCountedString::TakeString(&json_string));
}

bool InspectUI::HandleLocalXhrCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  std::string url = "http://localhost:" + path.substr(strlen(kLocalXhr));
  new ProtocolHttpRequest(Profile::FromWebUI(web_ui()), url,
      base::Bind(&InspectUI::RespondOnUIThread,
                 weak_factory_.GetWeakPtr(), callback));
  return true;
}

void InspectUI::RespondOnUIThread(
    const content::WebUIDataSource::GotDataCallback& callback,
    int result_code,
    const std::string& response) {
  ListValue result;
  result.AppendInteger(result_code);
  result.AppendString(response);
  std::string json_string;
  base::JSONWriter::Write(&result, &json_string);
  callback.Run(base::RefCountedString::TakeString(&json_string));
}
