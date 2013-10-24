// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inspect_ui.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/port_forwarding_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
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

const char kWorkerTargetType[]  = "worker";
const char kAdbTargetType[]  = "adb_page";

const char kInitUICommand[]  = "init-ui";
const char kInspectCommand[]  = "inspect";
const char kActivateCommand[]  = "activate";
const char kCloseCommand[]  = "close";
const char kReloadCommand[]  = "reload";
const char kOpenCommand[]  = "open";

const char kDiscoverUsbDevicesEnabledCommand[] =
    "set-discover-usb-devices-enabled";
const char kPortForwardingEnabledCommand[] =
    "set-port-forwarding-enabled";
const char kPortForwardingConfigCommand[] = "set-port-forwarding-config";

const char kPortForwardingDefaultPort[] = "8080";
const char kPortForwardingDefaultLocation[] = "localhost:8080";

const char kTargetIdField[]  = "id";
const char kTargetTypeField[]  = "type";
const char kAttachedField[]  = "attached";
const char kUrlField[]  = "url";
const char kNameField[]  = "name";
const char kFaviconUrlField[] = "faviconUrl";
const char kDescription[] = "description";
const char kAdbConnectedField[] = "adbConnected";
const char kAdbModelField[] = "adbModel";
const char kAdbSerialField[] = "adbSerial";
const char kAdbBrowserProductField[] = "adbBrowserProduct";
const char kAdbBrowserPackageField[] = "adbBrowserPackage";
const char kAdbBrowserVersionField[] = "adbBrowserVersion";
const char kAdbGlobalIdField[] = "adbGlobalId";
const char kAdbBrowsersField[] = "browsers";
const char kAdbPagesField[] = "pages";
const char kAdbPortStatus[] = "adbPortStatus";
const char kAdbScreenWidthField[] = "adbScreenWidth";
const char kAdbScreenHeightField[] = "adbScreenHeight";
const char kAdbAttachedForeignField[]  = "adbAttachedForeign";
const char kGuestList[] = "guests";

DictionaryValue* BuildTargetDescriptor(
    const DevToolsTargetImpl& target) {
  DictionaryValue* target_data = new DictionaryValue();
  target_data->SetString(kTargetIdField, target.GetId());
  target_data->SetString(kTargetTypeField, target.GetType());
  target_data->SetBoolean(kAttachedField, target.IsAttached());
  target_data->SetString(kUrlField, target.GetUrl().spec());
  target_data->SetString(kNameField, net::EscapeForHTML(target.GetTitle()));
  target_data->SetString(kFaviconUrlField, target.GetFaviconUrl().spec());
  target_data->SetString(kDescription, target.GetDescription());

  return target_data;
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
  void HandleActivateCommand(const ListValue* args);
  void HandleCloseCommand(const ListValue* args);
  void HandleReloadCommand(const ListValue* args);
  void HandleOpenCommand(const ListValue* args);
  void HandleBooleanPrefChanged(const char* pref_name,
                                const ListValue* args);
  void HandlePortForwardingConfigCommand(const ListValue* args);

  DevToolsTargetImpl* FindTarget(const ListValue* args);

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
  web_ui()->RegisterMessageCallback(kActivateCommand,
      base::Bind(&InspectMessageHandler::HandleActivateCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kCloseCommand,
      base::Bind(&InspectMessageHandler::HandleCloseCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDiscoverUsbDevicesEnabledCommand,
      base::Bind(&InspectMessageHandler::HandleBooleanPrefChanged,
                  base::Unretained(this),
                  &prefs::kDevToolsDiscoverUsbDevicesEnabled[0]));
  web_ui()->RegisterMessageCallback(kPortForwardingEnabledCommand,
      base::Bind(&InspectMessageHandler::HandleBooleanPrefChanged,
                 base::Unretained(this),
                 &prefs::kDevToolsPortForwardingEnabled[0]));
  web_ui()->RegisterMessageCallback(kPortForwardingConfigCommand,
      base::Bind(&InspectMessageHandler::HandlePortForwardingConfigCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kReloadCommand,
      base::Bind(&InspectMessageHandler::HandleReloadCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kOpenCommand,
      base::Bind(&InspectMessageHandler::HandleOpenCommand,
                 base::Unretained(this)));
}

void InspectMessageHandler::HandleInitUICommand(const ListValue*) {
  inspect_ui_->InitUI();
}

void InspectMessageHandler::HandleInspectCommand(const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;
  DevToolsTargetImpl* target = FindTarget(args);
  if (target)
    target->Inspect(profile);
}

void InspectMessageHandler::HandleActivateCommand(const ListValue* args) {
  DevToolsTargetImpl* target = FindTarget(args);
  if (target)
    target->Activate();
}

void InspectMessageHandler::HandleCloseCommand(const ListValue* args) {
  DevToolsTargetImpl* target = FindTarget(args);
  if (target)
    target->Close();
}

void InspectMessageHandler::HandleReloadCommand(const ListValue* args) {
  DevToolsTargetImpl* target = FindTarget(args);
  if (target)
    target->Reload();
}

void InspectMessageHandler::HandleOpenCommand(const ListValue* args) {
  if (args->GetSize() != 2)
    return;
  std::string browser_id;
  if (!args->GetString(0, &browser_id))
    return;
  scoped_refptr<DevToolsAdbBridge::RemoteBrowser> remote_browser =
      inspect_ui_->FindRemoteBrowser(browser_id);
  if (!remote_browser)
    return;
  std::string url;
  if (!args->GetString(1, &url))
    return;
  GURL gurl(url);
  if (!gurl.is_valid()) {
    gurl = GURL("http://" + url);
    if (!gurl.is_valid())
     return;
  }
  remote_browser->Open(gurl.spec());
}

DevToolsTargetImpl* InspectMessageHandler::FindTarget(const ListValue* args) {
  const DictionaryValue* data;
  std::string type;
  std::string id;
  if (args->GetSize() == 1 && args->GetDictionary(0, &data) &&
      data->GetString(kTargetTypeField, &type) &&
      data->GetString(kTargetIdField, &id)) {
    return inspect_ui_->FindTarget(type, id);
  }
  return NULL;
}

void InspectMessageHandler::HandleBooleanPrefChanged(
    const char* pref_name,
    const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  bool enabled;
  if (args->GetSize() == 1 && args->GetBoolean(0, &enabled))
    profile->GetPrefs()->SetBoolean(pref_name, enabled);
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
      public content::BrowserChildProcessObserver,
      public base::RefCountedThreadSafe<WorkerCreationDestructionListener> {
 public:
  WorkerCreationDestructionListener()
      : discovery_ui_(NULL) {}

  void Init(InspectUI* workers_ui) {
    DCHECK(workers_ui);
    DCHECK(!discovery_ui_);
    discovery_ui_ = workers_ui;
    BrowserChildProcessObserver::Add(this);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::RegisterObserver,
                   this));
  }

  void InspectUIDestroyed() {
    DCHECK(discovery_ui_);
    discovery_ui_ = NULL;
    BrowserChildProcessObserver::Remove(this);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WorkerCreationDestructionListener::UnregisterObserver,
                   this));
  }

  void UpdateUI() {
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

  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE {
    if (data.process_type == content::PROCESS_TYPE_WORKER)
      UpdateUI();
  }

  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE {
    if (data.process_type == content::PROCESS_TYPE_WORKER)
      UpdateUI();
  }

  void CollectWorkersData() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DevToolsTargetImpl::EnumerateWorkerTargets(
        base::Bind(
            &WorkerCreationDestructionListener::PopulateWorkersList, this));
  }

  void RegisterObserver() {
    WorkerService::GetInstance()->AddObserver(this);
  }

  void UnregisterObserver() {
    WorkerService::GetInstance()->RemoveObserver(this);
  }

  void PopulateWorkersList(const DevToolsTargetImpl::List& targets) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (discovery_ui_)
       discovery_ui_->PopulateWorkerTargets(targets);
  }

  InspectUI* discovery_ui_;
};

InspectUI::InspectUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new InspectMessageHandler(this));
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateInspectUIHTMLSource());

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
}

InspectUI::~InspectUI() {
  StopListeningNotifications();
}

void InspectUI::InitUI() {
  SetPortForwardingDefaults();
  StartListeningNotifications();
  PopulateWebContentsTargets();
  UpdateDiscoverUsbDevicesEnabled();
  UpdatePortForwardingEnabled();
  UpdatePortForwardingConfig();
  observer_->UpdateUI();
}

DevToolsTargetImpl* InspectUI::FindTarget(const std::string& type,
                                          const std::string& id) {
  if (type == kWorkerTargetType) {
    TargetMap::iterator it = worker_targets_.find(id);
    return it == worker_targets_.end() ? NULL : it->second;
  } else if (type == kAdbTargetType) {
    TargetMap::iterator it = remote_targets_.find(id);
    return it == remote_targets_.end() ? NULL : it->second;
  } else {
    TargetMap::iterator it = web_contents_targets_.find(id);
    return it == web_contents_targets_.end() ? NULL : it->second;
  }
}

scoped_refptr<DevToolsAdbBridge::RemoteBrowser>
InspectUI::FindRemoteBrowser(const std::string& id) {
  RemoteBrowsers::iterator it = remote_browsers_.find(id);
  return it == remote_browsers_.end() ? NULL : it->second;
}

void InspectUI::InspectDevices(Browser* browser) {
  content::RecordAction(content::UserMetricsAction("InspectDevices"));
  chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
      browser, GURL(chrome::kChromeUIInspectURL)));
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void InspectUI::PopulateWebContentsTargets() {
  ListValue list_value;

  std::map<WebContents*, DictionaryValue*> web_contents_to_descriptor_;
  std::vector<DevToolsTargetImpl*> guest_targets;

  DevToolsTargetImpl::List targets =
      DevToolsTargetImpl::EnumerateWebContentsTargets();

  STLDeleteValues(&web_contents_targets_);
  for (DevToolsTargetImpl::List::iterator it = targets.begin();
      it != targets.end(); ++it) {
    DevToolsTargetImpl* target = *it;
    WebContents* web_contents = target->GetWebContents();
    if (!web_contents)
      continue;
    RenderViewHost* rvh = web_contents->GetRenderViewHost();
    if (!rvh)
      continue;

    web_contents_targets_[target->GetId()] = target;
    if (rvh->GetProcess()->IsGuest()) {
      guest_targets.push_back(target);
    } else {
      DictionaryValue* descriptor = BuildTargetDescriptor(*target);
      list_value.Append(descriptor);
      web_contents_to_descriptor_[web_contents] = descriptor;
    }
  }

  // Add the list of guest-views to each of its embedders.
  for (std::vector<DevToolsTargetImpl*>::iterator it(guest_targets.begin());
       it != guest_targets.end(); ++it) {
    DevToolsTargetImpl* guest = (*it);
    WebContents* embedder = guest->GetWebContents()->GetEmbedderWebContents();
    if (embedder && web_contents_to_descriptor_.count(embedder) > 0) {
      DictionaryValue* parent = web_contents_to_descriptor_[embedder];
      ListValue* guests = NULL;
      if (!parent->GetList(kGuestList, &guests)) {
        guests = new ListValue();
        parent->Set(kGuestList, guests);
      }
      guests->Append(BuildTargetDescriptor(*guest));
    }
  }

  web_ui()->CallJavascriptFunction("populateWebContentsTargets", list_value);
}

void InspectUI::PopulateWorkerTargets(const DevToolsTargetImpl::List& targets) {
  ListValue list_value;

  STLDeleteValues(&worker_targets_);
  for (DevToolsTargetImpl::List::const_iterator it = targets.begin();
      it != targets.end(); ++it) {
    DevToolsTargetImpl* target = *it;
    list_value.Append(BuildTargetDescriptor(*target));
    worker_targets_[target->GetId()] = target;
  }

  web_ui()->CallJavascriptFunction("populateWorkerTargets", list_value);
}

void InspectUI::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (source != content::Source<WebContents>(web_ui()->GetWebContents()))
    PopulateWebContentsTargets();
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
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&InspectUI::UpdateDiscoverUsbDevicesEnabled,
                 base::Unretained(this)));
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
  Profile* profile = Profile::FromWebUI(web_ui());
  PortForwardingController* port_forwarding_controller =
      PortForwardingController::Factory::GetForProfile(profile);
  PortForwardingController::DevicesStatus port_forwarding_status;
  if (port_forwarding_controller)
    port_forwarding_status =
        port_forwarding_controller->UpdateDeviceList(*devices);

  remote_browsers_.clear();
  STLDeleteValues(&remote_targets_);
  ListValue device_list;
  for (DevToolsAdbBridge::RemoteDevices::iterator dit = devices->begin();
       dit != devices->end(); ++dit) {
    DevToolsAdbBridge::RemoteDevice* device = dit->get();
    DictionaryValue* device_data = new DictionaryValue();
    device_data->SetString(kAdbModelField, device->GetModel());
    device_data->SetString(kAdbSerialField, device->GetSerial());
    device_data->SetBoolean(kAdbConnectedField, device->IsConnected());
    std::string device_id = base::StringPrintf(
        "device:%s",
        device->GetSerial().c_str());
    device_data->SetString(kAdbGlobalIdField, device_id);
    ListValue* browser_list = new ListValue();
    device_data->Set(kAdbBrowsersField, browser_list);

    DevToolsAdbBridge::RemoteBrowsers& browsers = device->browsers();
    for (DevToolsAdbBridge::RemoteBrowsers::iterator bit =
        browsers.begin(); bit != browsers.end(); ++bit) {
      DevToolsAdbBridge::RemoteBrowser* browser = bit->get();
      DictionaryValue* browser_data = new DictionaryValue();
      browser_data->SetString(kAdbBrowserProductField, browser->product());
      browser_data->SetString(kAdbBrowserPackageField, browser->package());
      browser_data->SetString(kAdbBrowserVersionField, browser->version());
      std::string browser_id = base::StringPrintf(
          "browser:%s:%s:%s",
          device->GetSerial().c_str(),
          browser->product().c_str(),  // Force sorting by product name.
          browser->socket().c_str());
      browser_data->SetString(kAdbGlobalIdField, browser_id);
      remote_browsers_[browser_id] = browser;
      ListValue* page_list = new ListValue();
      browser_data->Set(kAdbPagesField, page_list);

      DevToolsTargetImpl::List pages = browser->CreatePageTargets();
      for (DevToolsTargetImpl::List::iterator it =
          pages.begin(); it != pages.end(); ++it) {
        DevToolsTargetImpl* page =  *it;
        DictionaryValue* page_data = BuildTargetDescriptor(*page);
        page_data->SetBoolean(
            kAdbAttachedForeignField,
            page->IsAttached() &&
                !DevToolsAdbBridge::HasDevToolsWindow(page->GetId()));
        // Pass the screen size in the page object to make sure that
        // the caching logic does not prevent the page item from updating
        // when the screen size changes.
        gfx::Size screen_size = device->screen_size();
        page_data->SetInteger(kAdbScreenWidthField, screen_size.width());
        page_data->SetInteger(kAdbScreenHeightField, screen_size.height());
        remote_targets_[page->GetId()] = page;
        page_list->Append(page_data);
      }
      browser_list->Append(browser_data);
    }

    if (port_forwarding_controller) {
      PortForwardingController::DevicesStatus::iterator sit =
          port_forwarding_status.find(device->GetSerial());
      if (sit != port_forwarding_status.end()) {
        DictionaryValue* port_status_dict = new DictionaryValue();
        typedef PortForwardingController::PortStatusMap StatusMap;
        const StatusMap& port_status = sit->second;
        for (StatusMap::const_iterator it = port_status.begin();
             it != port_status.end(); ++it) {
          port_status_dict->SetInteger(
              base::StringPrintf("%d", it->first), it->second);
        }
        device_data->Set(kAdbPortStatus, port_status_dict);
      }
    }

    device_list.Append(device_data);
  }
  web_ui()->CallJavascriptFunction("populateRemoteTargets", device_list);
}

void InspectUI::UpdateDiscoverUsbDevicesEnabled() {
  const Value* value = GetPrefValue(prefs::kDevToolsDiscoverUsbDevicesEnabled);
  web_ui()->CallJavascriptFunction("updateDiscoverUsbDevicesEnabled", *value);

  // Configure adb bridge.
  Profile* profile = Profile::FromWebUI(web_ui());
  DevToolsAdbBridge* adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(profile);
  if (adb_bridge) {
    bool enabled = false;
    value->GetAsBoolean(&enabled);

    DevToolsAdbBridge::DeviceProviders device_providers;
    device_providers.push_back(AndroidDeviceProvider::GetAdbDeviceProvider());

    if (enabled) {
      device_providers.push_back(
          AndroidDeviceProvider::GetUsbDeviceProvider(profile));
    }

    adb_bridge->set_device_providers(device_providers);
  }
}

void InspectUI::UpdatePortForwardingEnabled() {
  web_ui()->CallJavascriptFunction("updatePortForwardingEnabled",
      *GetPrefValue(prefs::kDevToolsPortForwardingEnabled));

}

void InspectUI::UpdatePortForwardingConfig() {
  web_ui()->CallJavascriptFunction("updatePortForwardingConfig",
      *GetPrefValue(prefs::kDevToolsPortForwardingConfig));
}

void InspectUI::SetPortForwardingDefaults() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  bool default_set;
  if (!GetPrefValue(prefs::kDevToolsPortForwardingDefaultSet)->
      GetAsBoolean(&default_set) || default_set) {
    return;
  }

  // This is the first chrome://inspect invocation on a fresh profile or after
  // upgrade from a version that did not have kDevToolsPortForwardingDefaultSet.
  prefs->SetBoolean(prefs::kDevToolsPortForwardingDefaultSet, true);

  bool enabled;
  const base::DictionaryValue* config;
  if (!GetPrefValue(prefs::kDevToolsPortForwardingEnabled)->
        GetAsBoolean(&enabled) ||
      !GetPrefValue(prefs::kDevToolsPortForwardingConfig)->
        GetAsDictionary(&config)) {
    return;
  }

  // Do nothing if user already took explicit action.
  if (enabled || config->size() != 0)
    return;

  base::DictionaryValue default_config;
  default_config.SetString(
      kPortForwardingDefaultPort, kPortForwardingDefaultLocation);
  prefs->Set(prefs::kDevToolsPortForwardingConfig, default_config);
}

const base::Value* InspectUI::GetPrefValue(const char* name) {
  Profile* profile = Profile::FromWebUI(web_ui());
  return profile->GetPrefs()->FindPreference(name)->GetValue();
}
