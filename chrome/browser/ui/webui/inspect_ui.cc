// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inspect_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/pref_names.h"
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
static const char kAdbPages[] = "adb-pages";

static const char kAppTargetType[] = "app";
static const char kExtensionTargetType[]  = "extension";
static const char kPageTargetType[]  = "page";
static const char kWorkerTargetType[]  = "worker";
static const char kAdbTargetType[]  = "adb_page";

static const char kInitUICommand[]  = "init-ui";
static const char kInspectCommand[]  = "inspect";
static const char kTerminateCommand[]  = "terminate";
static const char kPortForwardingEnabledCommand[] =
    "set-port-forwarding-enabled";
static const char kPortForwardingConfigCommand[] = "set-port-forwarding-config";

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
static const char kAdbBrowserNameField[] = "adbBrowserName";
static const char kAdbPageIdField[] = "adbPageId";
static const char kAdbBrowsersField[] = "browsers";
static const char kAdbPagesField[] = "pages";

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
        if (extension->is_hosted_app()
            || extension->is_legacy_packaged_app()
            || extension->is_platform_app())
          target_type = kAppTargetType;
        else
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

class InspectMessageHandler : public WebUIMessageHandler {
 public:
  explicit InspectMessageHandler(InspectUI* inspect_ui)
      : inspect_ui_(inspect_ui) {}
  virtual ~InspectMessageHandler() {}

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  void HandleInitUICommand(const ListValue* args);
  void HandleInspectCommand(const ListValue* args);
  void HandleTerminateCommand(const ListValue* args);
  void HandlePortForwardingEnabledCommand(const ListValue* args);
  void HandlePortForwardingConfigCommand(const ListValue* args);

  bool GetProcessAndRouteId(const ListValue* args,
                            int* process_id,
                            int* route_id);

  InspectUI* inspect_ui_;

  DISALLOW_COPY_AND_ASSIGN(InspectMessageHandler);
};

void InspectMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kInitUICommand,
      base::Bind(&InspectMessageHandler::HandleInitUICommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kInspectCommand,
      base::Bind(&InspectMessageHandler::HandleInspectCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kTerminateCommand,
      base::Bind(&InspectMessageHandler::HandleTerminateCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kPortForwardingEnabledCommand,
      base::Bind(&InspectMessageHandler::HandlePortForwardingEnabledCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kPortForwardingConfigCommand,
      base::Bind(&InspectMessageHandler::HandlePortForwardingConfigCommand,
                 base::Unretained(this)));
}

void InspectMessageHandler::HandleInitUICommand(const ListValue*) {
  inspect_ui_->InitUI();
}

void InspectMessageHandler::HandleInspectCommand(const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  int process_id;
  int route_id;
  if (!GetProcessAndRouteId(args, &process_id, &route_id) || process_id == 0
      || route_id == 0) {
    // Check for ADB page id
    const DictionaryValue* data;
    std::string page_id;
    if (args->GetSize() == 1 && args->GetDictionary(0, &data) &&
        data->GetString(kAdbPageIdField, &page_id)) {
      inspect_ui_->InspectRemotePage(page_id);
    }
    return;
  }

  RenderViewHost* rvh = RenderViewHost::FromID(process_id, route_id);
  if (rvh) {
    DevToolsWindow::OpenDevToolsWindow(rvh);
    return;
  }

  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForWorker(process_id, route_id));
  if (!agent_host.get())
    return;

  DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host.get());
}

static void TerminateWorker(int process_id, int route_id) {
  WorkerService::GetInstance()->TerminateWorker(process_id, route_id);
}

void InspectMessageHandler::HandleTerminateCommand(const ListValue* args) {
  int process_id;
  int route_id;
  if (!GetProcessAndRouteId(args, &process_id, &route_id))
    return;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TerminateWorker, process_id, route_id));
}

bool InspectMessageHandler::GetProcessAndRouteId(const ListValue* args,
                                                 int* process_id,
                                                 int* route_id) {
  const DictionaryValue* data;
  if (args->GetSize() == 1 && args->GetDictionary(0, &data) &&
      data->GetInteger(kProcessIdField, process_id) &&
      data->GetInteger(kRouteIdField, route_id)) {
    return true;
  }
  return false;
}

void InspectMessageHandler::HandlePortForwardingEnabledCommand(
    const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  bool enabled;
  if (args->GetSize() == 1 && args->GetBoolean(0, &enabled)) {
    profile->GetPrefs()->SetBoolean(
        prefs::kDevToolsPortForwardingEnabled, enabled);
  }
}

void InspectMessageHandler::HandlePortForwardingConfigCommand(
    const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  const DictionaryValue* dict_src;
  if (args->GetSize() == 1 && args->GetDictionary(0, &dict_src))
    profile->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig, *dict_src);
}

}  // namespace

class InspectUI::WorkerCreationDestructionListener
    : public WorkerServiceObserver,
      public base::RefCountedThreadSafe<WorkerCreationDestructionListener> {
 public:
  WorkerCreationDestructionListener()
      : discovery_ui_(NULL) {}

  void Init(InspectUI* workers_ui) {
    DCHECK(workers_ui);
    DCHECK(!discovery_ui_);
    discovery_ui_ = workers_ui;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::RegisterObserver,
                   this));
  }

  void InspectUIDestroyed() {
    DCHECK(discovery_ui_);
    discovery_ui_ = NULL;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::UnregisterObserver,
                   this));
  }

  void InitUI() {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::CollectWorkersData,
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
    CollectWorkersData();
  }

  virtual void WorkerDestroyed(int process_id, int route_id) OVERRIDE {
    CollectWorkersData();
  }

  void CollectWorkersData() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    scoped_ptr<ListValue> target_list(new ListValue());
    std::vector<WorkerService::WorkerInfo> worker_info =
        WorkerService::GetInstance()->GetWorkers();
    for (size_t i = 0; i < worker_info.size(); ++i) {
      target_list->Append(BuildTargetDescriptor(
          kWorkerTargetType,
          false,
          worker_info[i].url,
          UTF16ToUTF8(worker_info[i].name),
          GURL(),
          worker_info[i].process_id,
          worker_info[i].route_id,
          worker_info[i].handle));
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::PopulateWorkersList,
                   this, base::Owned(target_list.release())));
  }

  void RegisterObserver() {
    WorkerService::GetInstance()->AddObserver(this);
  }

  void UnregisterObserver() {
    WorkerService::GetInstance()->RemoveObserver(this);
  }

  void PopulateWorkersList(ListValue* target_list) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (discovery_ui_) {
      discovery_ui_->web_ui()->CallJavascriptFunction(
          "populateWorkersList", *target_list);
    }
  }

  InspectUI* discovery_ui_;
};

InspectUI::InspectUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new InspectMessageHandler(this));
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateInspectUIHTMLSource());
}

InspectUI::~InspectUI() {
  StopListeningNotifications();
}

void InspectUI::InitUI() {
  StartListeningNotifications();
  PopulateLists();
  UpdatePortForwardingEnabled();
  UpdatePortForwardingConfig();
  observer_->InitUI();
}

void InspectUI::InspectRemotePage(const std::string& id) {
  RemotePages::iterator it = remote_pages_.find(id);
  if (it != remote_pages_.end()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    it->second->Inspect(profile);
  }
}

void InspectUI::PopulateLists() {
  std::set<RenderViewHost*> tab_rvhs;
  for (TabContentsIterator it; !it.done(); it.Next())
    tab_rvhs.insert(it->GetRenderViewHost());

  scoped_ptr<ListValue> target_list(new ListValue());

  std::vector<RenderViewHost*> rvh_vector =
      DevToolsAgentHost::GetValidRenderViewHosts();

  for (std::vector<RenderViewHost*>::iterator it(rvh_vector.begin());
       it != rvh_vector.end(); it++) {
    bool is_tab = tab_rvhs.find(*it) != tab_rvhs.end();
    target_list->Append(BuildTargetDescriptor(*it, is_tab));
  }
  web_ui()->CallJavascriptFunction("populateLists", *target_list.get());
}

void InspectUI::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (source != content::Source<WebContents>(web_ui()->GetWebContents()))
    PopulateLists();
  else if (type == content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED)
    StopListeningNotifications();
}

void InspectUI::StartListeningNotifications() {
  if (observer_)
    return;

  observer_ = new WorkerCreationDestructionListener();
  observer_->Init(this);

  Profile* profile = Profile::FromWebUI(web_ui());
  DevToolsAdbBridge* adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(profile);
  if (adb_bridge)
    adb_bridge->AddListener(this);

  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                              content::NotificationService::AllSources());

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled,
      base::Bind(&InspectUI::UpdatePortForwardingEnabled,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig,
      base::Bind(&InspectUI::UpdatePortForwardingConfig,
                 base::Unretained(this)));
}

void InspectUI::StopListeningNotifications()
{
  if (!observer_.get())
    return;
  Profile* profile = Profile::FromWebUI(web_ui());
  DevToolsAdbBridge* adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(profile);
  if (adb_bridge)
    adb_bridge->RemoveListener(this);
  observer_->InspectUIDestroyed();
  observer_ = NULL;
  notification_registrar_.RemoveAll();
  pref_change_registrar_.RemoveAll();
}

content::WebUIDataSource* InspectUI::CreateInspectUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIInspectHost);
  source->AddResourcePath("inspect.css", IDR_INSPECT_CSS);
  source->AddResourcePath("inspect.js", IDR_INSPECT_JS);
  source->SetDefaultResource(IDR_INSPECT_HTML);
  return source;
}

void InspectUI::RemoteDevicesChanged(
    DevToolsAdbBridge::RemoteDevices* devices) {
  remote_pages_.clear();
  ListValue device_list;
  for (DevToolsAdbBridge::RemoteDevices::iterator dit = devices->begin();
       dit != devices->end(); ++dit) {
    DevToolsAdbBridge::RemoteDevice& device = *(dit->get());
    DictionaryValue* device_data = new DictionaryValue();
    device_data->SetString(kAdbModelField, device.model());
    device_data->SetString(kAdbSerialField, device.serial());
    ListValue* browser_list = new ListValue();
    device_data->Set(kAdbBrowsersField, browser_list);

    DevToolsAdbBridge::RemoteBrowsers& browsers = device.browsers();
    for (DevToolsAdbBridge::RemoteBrowsers::iterator bit =
        browsers.begin(); bit != browsers.end(); ++bit) {
      DevToolsAdbBridge::RemoteBrowser& browser = *(bit->get());
      DictionaryValue* browser_data = new DictionaryValue();
      browser_data->SetString(kAdbBrowserNameField, browser.name());
      ListValue* page_list = new ListValue();
      browser_data->Set(kAdbPagesField, page_list);

      DevToolsAdbBridge::RemotePages& pages = browser.pages();
      for (DevToolsAdbBridge::RemotePages::iterator it =
          pages.begin(); it != pages.end(); ++it) {
        DevToolsAdbBridge::RemotePage* page =  it->get();
        DictionaryValue* page_data = BuildTargetDescriptor(kAdbTargetType,
            false, GURL(page->url()), page->title(), GURL(page->favicon_url()),
            0, 0);
        page_data->SetString(kAdbPageIdField, page->global_id());
        page_list->Append(page_data);
        remote_pages_[page->global_id()] = page;
      }
      browser_list->Append(browser_data);
    }
    device_list.Append(device_data);
  }
  web_ui()->CallJavascriptFunction("populateDeviceLists", device_list);
}

void InspectUI::UpdatePortForwardingEnabled() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const base::Value* value = profile->GetPrefs()->FindPreference(
      prefs::kDevToolsPortForwardingEnabled)->GetValue();
  web_ui()->CallJavascriptFunction("updatePortForwardingEnabled", *value);

}

void InspectUI::UpdatePortForwardingConfig() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const base::Value* value = profile->GetPrefs()->FindPreference(
      prefs::kDevToolsPortForwardingConfig)->GetValue();
  web_ui()->CallJavascriptFunction("updatePortForwardingConfig", *value);
}
