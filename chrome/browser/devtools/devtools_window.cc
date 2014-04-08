// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::DictionaryValue;
using blink::WebInputEvent;
using content::BrowserThread;
using content::DevToolsAgentHost;


// DevToolsConfirmInfoBarDelegate ---------------------------------------------

class DevToolsConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // If |infobar_service| is NULL, runs |callback| with a single argument with
  // value "false".  Otherwise, creates a dev tools confirm infobar and delegate
  // and adds the infobar to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const DevToolsWindow::InfoBarCallback& callback,
                     const base::string16& message);

 private:
  DevToolsConfirmInfoBarDelegate(
      const DevToolsWindow::InfoBarCallback& callback,
      const base::string16& message);
  virtual ~DevToolsConfirmInfoBarDelegate();

  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  DevToolsWindow::InfoBarCallback callback_;
  const base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsConfirmInfoBarDelegate);
};

void DevToolsConfirmInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const DevToolsWindow::InfoBarCallback& callback,
    const base::string16& message) {
  if (!infobar_service) {
    callback.Run(false);
    return;
  }

  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new DevToolsConfirmInfoBarDelegate(callback, message))));
}

DevToolsConfirmInfoBarDelegate::DevToolsConfirmInfoBarDelegate(
    const DevToolsWindow::InfoBarCallback& callback,
    const base::string16& message)
    : ConfirmInfoBarDelegate(),
      callback_(callback),
      message_(message) {
}

DevToolsConfirmInfoBarDelegate::~DevToolsConfirmInfoBarDelegate() {
  if (!callback_.is_null())
    callback_.Run(false);
}

base::string16 DevToolsConfirmInfoBarDelegate::GetMessageText() const {
  return message_;
}

base::string16 DevToolsConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_DEV_TOOLS_CONFIRM_ALLOW_BUTTON : IDS_DEV_TOOLS_CONFIRM_DENY_BUTTON);
}

bool DevToolsConfirmInfoBarDelegate::Accept() {
  callback_.Run(true);
  callback_.Reset();
  return true;
}

bool DevToolsConfirmInfoBarDelegate::Cancel() {
  callback_.Run(false);
  callback_.Reset();
  return true;
}

// DevToolsEventForwarder -----------------------------------------------------

namespace {

static const char kKeyUpEventName[] = "keyup";
static const char kKeyDownEventName[] = "keydown";

}  // namespace

class DevToolsEventForwarder {
 public:
  explicit DevToolsEventForwarder(DevToolsWindow* window)
     : devtools_window_(window) {}

  // Registers whitelisted shortcuts with the forwarder.
  // Only registered keys will be forwarded to the DevTools frontend.
  void SetWhitelistedShortcuts(const std::string& message);

  // Forwards a keyboard event to the DevTools frontend if it is whitelisted.
  // Returns |true| if the event has been forwarded, |false| otherwise.
  bool ForwardEvent(const content::NativeWebKeyboardEvent& event);

 private:
  static int VirtualKeyCodeWithoutLocation(int key_code);
  static bool KeyWhitelistingAllowed(int key_code, int modifiers);
  static int CombineKeyCodeAndModifiers(int key_code, int modifiers);

  DevToolsWindow* devtools_window_;
  std::set<int> whitelisted_keys_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsEventForwarder);
};

void DevToolsEventForwarder::SetWhitelistedShortcuts(
    const std::string& message) {
  scoped_ptr<base::Value> parsed_message(base::JSONReader::Read(message));
  base::ListValue* shortcut_list;
  if (!parsed_message->GetAsList(&shortcut_list))
      return;
  base::ListValue::iterator it = shortcut_list->begin();
  for (; it != shortcut_list->end(); ++it) {
    base::DictionaryValue* dictionary;
    if (!(*it)->GetAsDictionary(&dictionary))
      continue;
    int key_code = 0;
    dictionary->GetInteger("keyCode", &key_code);
    if (key_code == 0)
      continue;
    int modifiers = 0;
    dictionary->GetInteger("modifiers", &modifiers);
    if (!KeyWhitelistingAllowed(key_code, modifiers)) {
      LOG(WARNING) << "Key whitelisting forbidden: "
                   << "(" << key_code << "," << modifiers << ")";
      continue;
    }
    whitelisted_keys_.insert(CombineKeyCodeAndModifiers(key_code, modifiers));
  }
}

bool DevToolsEventForwarder::ForwardEvent(
    const content::NativeWebKeyboardEvent& event) {
  std::string event_type;
  switch (event.type) {
    case WebInputEvent::KeyDown:
    case WebInputEvent::RawKeyDown:
      event_type = kKeyDownEventName;
      break;
    case WebInputEvent::KeyUp:
      event_type = kKeyUpEventName;
      break;
    default:
      return false;
  }

  int key_code = VirtualKeyCodeWithoutLocation(event.windowsKeyCode);
  int key = CombineKeyCodeAndModifiers(key_code, event.modifiers);
  if (whitelisted_keys_.find(key) == whitelisted_keys_.end())
    return false;

  base::DictionaryValue event_data;
  event_data.SetString("type", event_type);
  event_data.SetString("keyIdentifier", event.keyIdentifier);
  event_data.SetInteger("keyCode", key_code);
  event_data.SetInteger("modifiers", event.modifiers);
  devtools_window_->CallClientFunction(
      "InspectorFrontendAPI.keyEventUnhandled", &event_data, NULL, NULL);
  return true;
}

int DevToolsEventForwarder::CombineKeyCodeAndModifiers(int key_code,
                                                       int modifiers) {
  return key_code | (modifiers << 16);
}

bool DevToolsEventForwarder::KeyWhitelistingAllowed(int key_code,
                                                    int modifiers) {
  return (ui::VKEY_F1 <= key_code && key_code <= ui::VKEY_F12) ||
      modifiers != 0;
}

// Mapping copied from Blink's KeyboardEvent.cpp.
int DevToolsEventForwarder::VirtualKeyCodeWithoutLocation(int key_code)
{
  switch (key_code) {
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
        return ui::VKEY_CONTROL;
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
        return ui::VKEY_SHIFT;
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
        return ui::VKEY_MENU;
    default:
        return key_code;
  }
}

// DevToolsWindow::InspectedWebContentsObserver -------------------------------

class DevToolsWindow::InspectedWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit InspectedWebContentsObserver(content::WebContents* web_contents);
  virtual ~InspectedWebContentsObserver();

  content::WebContents* web_contents() {
    return WebContentsObserver::web_contents();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InspectedWebContentsObserver);
};

DevToolsWindow::InspectedWebContentsObserver::InspectedWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

DevToolsWindow::InspectedWebContentsObserver::~InspectedWebContentsObserver() {
}


// DevToolsWindow::FrontendWebContentsObserver --------------------------------

class DevToolsWindow::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(DevToolsWindow* window);
  virtual ~FrontendWebContentsObserver();

 private:
  // contents::WebContentsObserver:
  virtual void AboutToNavigateRenderView(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame(int32 page_id) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents*) OVERRIDE;

  DevToolsWindow* devtools_window_;
  DISALLOW_COPY_AND_ASSIGN(FrontendWebContentsObserver);
};

DevToolsWindow::FrontendWebContentsObserver::FrontendWebContentsObserver(
    DevToolsWindow* devtools_window)
    : WebContentsObserver(devtools_window->web_contents()),
      devtools_window_(devtools_window) {
}

void DevToolsWindow::FrontendWebContentsObserver::WebContentsDestroyed(
    content::WebContents* contents) {
  delete devtools_window_;
}

DevToolsWindow::FrontendWebContentsObserver::~FrontendWebContentsObserver() {
}

void DevToolsWindow::FrontendWebContentsObserver::AboutToNavigateRenderView(
    content::RenderViewHost* render_view_host) {
  content::DevToolsClientHost::SetupDevToolsFrontendClient(render_view_host);
}

void DevToolsWindow::FrontendWebContentsObserver::
    DocumentOnLoadCompletedInMainFrame(int32 page_id) {
  devtools_window_->DocumentOnLoadCompletedInMainFrame();
}

// DevToolsWindow -------------------------------------------------------------

namespace {

typedef std::vector<DevToolsWindow*> DevToolsWindows;
base::LazyInstance<DevToolsWindows>::Leaky g_instances =
    LAZY_INSTANCE_INITIALIZER;

static const char kFrontendHostId[] = "id";
static const char kFrontendHostMethod[] = "method";
static const char kFrontendHostParams[] = "params";

std::string SkColorToRGBAString(SkColor color) {
  // We avoid StringPrintf because it will use locale specific formatters for
  // the double (e.g. ',' instead of '.' in German).
  return "rgba(" + base::IntToString(SkColorGetR(color)) + "," +
      base::IntToString(SkColorGetG(color)) + "," +
      base::IntToString(SkColorGetB(color)) + "," +
      base::DoubleToString(SkColorGetA(color) / 255.0) + ")";
}

base::DictionaryValue* CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  base::DictionaryValue* file_system_value = new base::DictionaryValue();
  file_system_value->SetString("fileSystemName", file_system.file_system_name);
  file_system_value->SetString("rootURL", file_system.root_url);
  file_system_value->SetString("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

}  // namespace

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

DevToolsWindow::~DevToolsWindow() {
  content::DevToolsManager::GetInstance()->ClientHostClosing(
      frontend_host_.get());
  UpdateBrowserToolbar();

  DevToolsWindows* instances = g_instances.Pointer();
  DevToolsWindows::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
}

// static
std::string DevToolsWindow::GetDevToolsWindowPlacementPrefKey() {
  return std::string(prefs::kBrowserWindowPlacement) + "_" +
      std::string(kDevToolsApp);
}

// static
void DevToolsWindow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kDevToolsEditedFiles,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsFileSystemPaths,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDevToolsAdbKey, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterDictionaryPref(
      GetDevToolsWindowPlacementPrefKey().c_str(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kDevToolsDiscoverUsbDevicesEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDevToolsPortForwardingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDevToolsPortForwardingDefaultSet,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsPortForwardingConfig,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
DevToolsWindow* DevToolsWindow::GetDockedInstanceForInspectedTab(
    content::WebContents* inspected_web_contents) {
  DevToolsWindow* window = GetInstanceForInspectedRenderViewHost(
      inspected_web_contents->GetRenderViewHost());
  if (!window)
    return NULL;
  // Not yet loaded window is treated as docked, but we should not present it
  // until we decided on docking.
  bool is_docked_set = window->load_state_ == kLoadCompleted ||
      window->load_state_ == kIsDockedSet;
  return window->is_docked_ && is_docked_set ? window : NULL;
}

// static
DevToolsWindow* DevToolsWindow::GetInstanceForInspectedRenderViewHost(
      content::RenderViewHost* inspected_rvh) {
  if (!inspected_rvh || !DevToolsAgentHost::HasFor(inspected_rvh))
    return NULL;

  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetOrCreateFor(
      inspected_rvh));
  return FindDevToolsWindow(agent.get());
}

// static
DevToolsWindow* DevToolsWindow::GetInstanceForInspectedWebContents(
    content::WebContents* inspected_web_contents) {
  if (!inspected_web_contents)
    return NULL;
  return GetInstanceForInspectedRenderViewHost(
      inspected_web_contents->GetRenderViewHost());
}

// static
bool DevToolsWindow::IsDevToolsWindow(content::RenderViewHost* window_rvh) {
  return AsDevToolsWindow(window_rvh) != NULL;
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    DevToolsAgentHost* worker_agent) {
  DevToolsWindow* window = FindDevToolsWindow(worker_agent);
  if (!window) {
    window = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    // Will disconnect the current client host if there is one.
    content::DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        worker_agent, window->frontend_host_.get());
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
  return window;
}

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindowForWorker(
    Profile* profile) {
  content::RecordAction(base::UserMetricsAction("DevTools_InspectWorker"));
  return Create(profile, GURL(), NULL, true, false, false);
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    content::RenderViewHost* inspected_rvh) {
  return ToggleDevToolsWindow(
      inspected_rvh, true, DevToolsToggleAction::Show());
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    content::RenderViewHost* inspected_rvh,
    const DevToolsToggleAction& action) {
  return ToggleDevToolsWindow(
      inspected_rvh, true, action);
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindowForTest(
    content::RenderViewHost* inspected_rvh,
    bool is_docked) {
  DevToolsWindow* window = OpenDevToolsWindow(inspected_rvh);
  window->SetIsDockedAndShowImmediatelyForTest(is_docked);
  return window;
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindowForTest(
    Browser* browser,
    bool is_docked) {
  return OpenDevToolsWindowForTest(
      browser->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost(),
      is_docked);
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    Browser* browser,
    const DevToolsToggleAction& action) {
  if (action.type() == DevToolsToggleAction::kToggle &&
      browser->is_devtools()) {
    browser->tab_strip_model()->CloseAllTabs();
    return NULL;
  }

  return ToggleDevToolsWindow(
      browser->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost(),
      action.type() == DevToolsToggleAction::kInspect, action);
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host);
  if (!window) {
    window = Create(profile, DevToolsUI::GetProxyURL(frontend_url), NULL,
                    false, true, false);
    content::DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host, window->frontend_host_.get());
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    content::RenderViewHost* inspected_rvh,
    bool force_open,
    const DevToolsToggleAction& action) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_rvh->GetProcess()->GetBrowserContext());
    content::RecordAction(
        base::UserMetricsAction("DevTools_InspectRenderer"));
    window = Create(profile, GURL(), inspected_rvh, false, false, true);
    manager->RegisterDevToolsClientHostFor(agent.get(),
                                           window->frontend_host_.get());
    do_open = true;
  }

  // Update toolbar to reflect DevTools changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it.
  if (!window->is_docked_ || do_open)
    window->ScheduleShow(action);
  else
    window->CloseWindow();

  return window;
}

// static
void DevToolsWindow::InspectElement(content::RenderViewHost* inspected_rvh,
                                    int x,
                                    int y) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  agent->InspectElement(x, y);
  bool should_measure_time = FindDevToolsWindow(agent.get()) == NULL;
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  DevToolsWindow* window = OpenDevToolsWindow(inspected_rvh);
  if (should_measure_time)
    window->inspect_element_start_time_ = start_time;
}

void DevToolsWindow::InspectedContentsClosing() {
  intercepted_page_beforeunload_ = false;
  // This will prevent any activity after frontend is loaded.
  action_on_load_ = DevToolsToggleAction::NoOp();
  ignore_set_is_docked_ = true;
  web_contents_->GetRenderViewHost()->ClosePage();
}

content::RenderViewHost* DevToolsWindow::GetRenderViewHost() {
  return web_contents_->GetRenderViewHost();
}

const DevToolsContentsResizingStrategy&
DevToolsWindow::GetContentsResizingStrategy() const {
  return contents_resizing_strategy_;
}

gfx::Size DevToolsWindow::GetMinimumSize() const {
  const gfx::Size kMinDevToolsSize = gfx::Size(230, 100);
  return kMinDevToolsSize;
}

void DevToolsWindow::ScheduleShow(const DevToolsToggleAction& action) {
  if (load_state_ == kLoadCompleted) {
    Show(action);
    return;
  }

  // Action will be done only after load completed.
  action_on_load_ = action;

  if (!can_dock_) {
    // No harm to show always-undocked window right away.
    is_docked_ = false;
    Show(DevToolsToggleAction::Show());
  }
}

void DevToolsWindow::Show(const DevToolsToggleAction& action) {
  if (action.type() == DevToolsToggleAction::kNoOp)
    return;

  if (is_docked_) {
    DCHECK(can_dock_);
    Browser* inspected_browser = NULL;
    int inspected_tab_index = -1;
    FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                    &inspected_browser,
                                    &inspected_tab_index);
    DCHECK(inspected_browser);
    DCHECK(inspected_tab_index != -1);

    // Tell inspected browser to update splitter and switch to inspected panel.
    BrowserWindow* inspected_window = inspected_browser->window();
    web_contents_->SetDelegate(this);

    TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
    tab_strip_model->ActivateTabAt(inspected_tab_index, true);

    inspected_window->UpdateDevTools();
    web_contents_->GetView()->SetInitialFocus();
    inspected_window->Show();
    // On Aura, focusing once is not enough. Do it again.
    // Note that focusing only here but not before isn't enough either. We just
    // need to focus twice.
    web_contents_->GetView()->SetInitialFocus();

    PrefsTabHelper::CreateForWebContents(web_contents_);
    GetRenderViewHost()->SyncRendererPrefs();

    DoAction(action);
    return;
  }

  // Avoid consecutive window switching if the devtools window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || (action.type() != DevToolsToggleAction::kInspect);

  if (!browser_)
    CreateDevToolsBrowser();

  if (should_show_window) {
    browser_->window()->Show();
    web_contents_->GetView()->SetInitialFocus();
  }

  DoAction(action);
}

// static
bool DevToolsWindow::HandleBeforeUnload(content::WebContents* frontend_contents,
    bool proceed, bool* proceed_to_fire_unload) {
  DevToolsWindow* window = AsDevToolsWindow(
      frontend_contents->GetRenderViewHost());
  if (!window)
    return false;
  if (!window->intercepted_page_beforeunload_)
    return false;
  window->BeforeUnloadFired(frontend_contents, proceed,
      proceed_to_fire_unload);
  return true;
}

// static
bool DevToolsWindow::InterceptPageBeforeUnload(content::WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedRenderViewHost(
          contents->GetRenderViewHost());
  if (!window || window->intercepted_page_beforeunload_)
    return false;

  // Not yet loaded frontend will not handle beforeunload.
  if (window->load_state_ != kLoadCompleted)
    return false;

  window->intercepted_page_beforeunload_ = true;
  // Handle case of devtools inspecting another devtools instance by passing
  // the call up to the inspecting devtools instance.
  if (!DevToolsWindow::InterceptPageBeforeUnload(window->web_contents())) {
    window->web_contents()->DispatchBeforeUnload(false);
  }
  return true;
}

// static
bool DevToolsWindow::NeedsToInterceptBeforeUnload(
    content::WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedRenderViewHost(
          contents->GetRenderViewHost());
  return window && !window->intercepted_page_beforeunload_;
}

// static
bool DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(
    Browser* browser) {
  DCHECK(browser->is_devtools());
  // When FastUnloadController is used, devtools frontend will be detached
  // from the browser window at this point which means we've already fired
  // beforeunload.
  if (browser->tab_strip_model()->empty())
    return true;
  content::WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow* window = AsDevToolsWindow(contents->GetRenderViewHost());
  if (!window)
    return false;
  return window->intercepted_page_beforeunload_;
}

// static
void DevToolsWindow::OnPageCloseCanceled(content::WebContents* contents) {
  DevToolsWindow *window =
      DevToolsWindow::GetInstanceForInspectedRenderViewHost(
          contents->GetRenderViewHost());
  if (!window)
    return;
  window->intercepted_page_beforeunload_ = false;
  // Propagate to devtools opened on devtools if any.
  DevToolsWindow::OnPageCloseCanceled(window->web_contents());
}

DevToolsWindow::DevToolsWindow(Profile* profile,
                               const GURL& url,
                               content::RenderViewHost* inspected_rvh,
                               bool can_dock)
    : profile_(profile),
      browser_(NULL),
      is_docked_(true),
      can_dock_(can_dock),
      // This initialization allows external front-end to work without changes.
      // We don't wait for docking call, but instead immediately show undocked.
      // Passing "dockSide=undocked" parameter ensures proper UI.
      load_state_(can_dock ? kNotLoaded : kIsDockedSet),
      action_on_load_(DevToolsToggleAction::NoOp()),
      ignore_set_is_docked_(false),
      intercepted_page_beforeunload_(false),
      weak_factory_(this) {
  web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  frontend_contents_observer_.reset(new FrontendWebContentsObserver(this));
  web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;

  // Set up delegate, so we get fully-functional window immediately.
  // It will not appear in UI though until |load_state_ == kLoadCompleted|.
  web_contents_->SetDelegate(this);

  web_contents_->GetController().LoadURL(url, content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  frontend_host_.reset(content::DevToolsClientHost::CreateDevToolsFrontendHost(
      web_contents_, this));
  file_helper_.reset(new DevToolsFileHelper(web_contents_, profile));
  file_system_indexer_ = new DevToolsFileSystemIndexer();
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_);

  g_instances.Get().push_back(this);

  // Wipe out page icon so that the default application icon is used.
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  entry->GetFavicon().image = gfx::Image();
  entry->GetFavicon().valid = true;

  // Register on-load actions.
  registrar_.Add(
      this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));

  // There is no inspected_rvh in case of shared workers.
  if (inspected_rvh)
    inspected_contents_observer_.reset(new InspectedWebContentsObserver(
        content::WebContents::FromRenderViewHost(inspected_rvh)));

  embedder_message_dispatcher_.reset(
      DevToolsEmbedderMessageDispatcher::createForDevToolsFrontend(this));
  event_forwarder_.reset(new DevToolsEventForwarder(this));
}

// static
DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    const GURL& frontend_url,
    content::RenderViewHost* inspected_rvh,
    bool shared_worker_frontend,
    bool external_frontend,
    bool can_dock) {
  if (inspected_rvh) {
    // Check for a place to dock.
    Browser* browser = NULL;
    int tab;
    content::WebContents* inspected_web_contents =
        content::WebContents::FromRenderViewHost(inspected_rvh);
    if (!FindInspectedBrowserAndTabIndex(inspected_web_contents,
                                         &browser, &tab) ||
        inspected_rvh->GetMainFrame()->IsCrossProcessSubframe() ||
        browser->is_type_popup()) {
      can_dock = false;
    }
  }

  // Create WebContents with devtools.
  GURL url(GetDevToolsURL(profile, frontend_url,
                          shared_worker_frontend,
                          external_frontend,
                          can_dock));
  return new DevToolsWindow(profile, url, inspected_rvh, can_dock);
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    const GURL& base_url,
                                    bool shared_worker_frontend,
                                    bool external_frontend,
                                    bool can_dock) {
  if (base_url.SchemeIs("data"))
    return base_url;

  std::string frontend_url(
      base_url.is_empty() ? chrome::kChromeUIDevToolsURL : base_url.spec());
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile);
  DCHECK(tp);
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
      "dockSide=undocked" + // TODO(dgozman): remove this support in M38.
      "&toolbarColor=" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "&textColor=" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)));
  if (shared_worker_frontend)
    url_string += "&isSharedWorker=true";
  if (external_frontend)
    url_string += "&remoteFrontend=true";
  if (can_dock)
    url_string += "&can_dock=true";
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDevToolsExperiments))
    url_string += "&experiments=true";
  url_string += "&updateAppcache";
  return GURL(url_string);
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  DevToolsWindows* instances = g_instances.Pointer();
  content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if (manager->GetDevToolsAgentHostFor((*it)->frontend_host_.get()) ==
        agent_host)
      return *it;
  }
  return NULL;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    content::RenderViewHost* window_rvh) {
  if (g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->web_contents_->GetRenderViewHost() == window_rvh)
      return *it;
  }
  return NULL;
}

void DevToolsWindow::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  UpdateTheme();
}

content::WebContents* DevToolsWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == web_contents_);
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme)) {
    content::WebContents* inspected_web_contents = GetInspectedWebContents();
    return inspected_web_contents ?
        inspected_web_contents->OpenURL(params) : NULL;
  }

  content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
  scoped_refptr<DevToolsAgentHost> agent_host(
      manager->GetDevToolsAgentHostFor(frontend_host_.get()));
  if (!agent_host.get())
    return NULL;
  manager->ClientHostClosing(frontend_host_.get());
  manager->RegisterDevToolsClientHostFor(agent_host.get(),
                                         frontend_host_.get());

  content::NavigationController::LoadURLParams load_url_params(params.url);
  web_contents_->GetController().LoadURLWithParams(load_url_params);
  return web_contents_;
}

void DevToolsWindow::AddNewContents(content::WebContents* source,
                                    content::WebContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture,
                                    bool* was_blocked) {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, new_contents, disposition, initial_pos, user_gesture,
        was_blocked);
  }
}

void DevToolsWindow::CloseContents(content::WebContents* source) {
  CHECK(is_docked_);
  // This will prevent any activity after frontend is loaded.
  action_on_load_ = DevToolsToggleAction::NoOp();
  ignore_set_is_docked_ = true;
  // Update dev tools to reflect removed dev tools window.
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
  // In case of docked web_contents_, we own it so delete here.
  // Embedding DevTools window will be deleted as a result of
  // WebContentsDestroyed callback.
  delete web_contents_;
}

void DevToolsWindow::ContentsZoomChange(bool zoom_in) {
  DCHECK(is_docked_);
  chrome_page_zoom::Zoom(web_contents(),
      zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
}

void DevToolsWindow::BeforeUnloadFired(content::WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (!intercepted_page_beforeunload_) {
    // Docked devtools window closed directly.
    if (proceed) {
      content::DevToolsManager::GetInstance()->ClientHostClosing(
          frontend_host_.get());
    }
    *proceed_to_fire_unload = proceed;
  } else {
    // Inspected page is attempting to close.
    content::WebContents* inspected_web_contents = GetInspectedWebContents();
    if (proceed) {
      inspected_web_contents->DispatchBeforeUnload(false);
    } else {
      bool should_proceed;
      inspected_web_contents->GetDelegate()->BeforeUnloadFired(
          inspected_web_contents, false, &should_proceed);
      DCHECK(!should_proceed);
    }
    *proceed_to_fire_unload = false;
  }
}

bool DevToolsWindow::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  if (is_docked_) {
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window) {
      return inspected_window->PreHandleKeyboardEvent(event,
                                                      is_keyboard_shortcut);
    }
  }
  return false;
}

void DevToolsWindow::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (is_docked_) {
    if (event.windowsKeyCode == 0x08) {
      // Do not navigate back in history on Windows (http://crbug.com/74156).
      return;
    }
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->HandleKeyboardEvent(event);
  }
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  return (inspected_web_contents && inspected_web_contents->GetDelegate()) ?
      inspected_web_contents->GetDelegate()->GetJavaScriptDialogManager() :
      content::WebContentsDelegate::GetJavaScriptDialogManager();
}

content::ColorChooser* DevToolsWindow::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void DevToolsWindow::RunFileChooser(content::WebContents* web_contents,
                                    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void DevToolsWindow::WebContentsFocused(content::WebContents* contents) {
  Browser* inspected_browser = NULL;
  int inspected_tab_index = -1;
  if (is_docked_ && FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                                    &inspected_browser,
                                                    &inspected_tab_index))
    inspected_browser->window()->WebContentsFocused(contents);
}

bool DevToolsWindow::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void DevToolsWindow::DispatchOnEmbedder(const std::string& message) {
  std::string method;
  base::ListValue empty_params;
  base::ListValue* params = &empty_params;

  base::DictionaryValue* dict = NULL;
  scoped_ptr<base::Value> parsed_message(base::JSONReader::Read(message));
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString(kFrontendHostMethod, &method) ||
      (dict->HasKey(kFrontendHostParams) &&
          !dict->GetList(kFrontendHostParams, &params))) {
    LOG(ERROR) << "Invalid message was sent to embedder: " << message;
    return;
  }

  int id = 0;
  dict->GetInteger(kFrontendHostId, &id);

  std::string error;
  embedder_message_dispatcher_->Dispatch(method, params, &error);
  if (id) {
    scoped_ptr<base::Value> id_value(base::Value::CreateIntegerValue(id));
    scoped_ptr<base::Value> error_value(base::Value::CreateStringValue(error));
    CallClientFunction("InspectorFrontendAPI.embedderMessageAck",
                       id_value.get(), error_value.get(), NULL);
  }
}

void DevToolsWindow::ActivateWindow() {
  if (is_docked_ && GetInspectedBrowserWindow())
    web_contents_->GetView()->Focus();
  else if (!is_docked_ && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void DevToolsWindow::ActivateContents(content::WebContents* contents) {
  if (is_docked_) {
    content::WebContents* inspected_tab = this->GetInspectedWebContents();
    inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else {
    browser_->window()->Activate();
  }
}

void DevToolsWindow::CloseWindow() {
  DCHECK(is_docked_);
  // This will prevent any activity after frontend is loaded.
  action_on_load_ = DevToolsToggleAction::NoOp();
  ignore_set_is_docked_ = true;
  web_contents_->DispatchBeforeUnload(false);
}

void DevToolsWindow::SetContentsInsets(
    int top, int left, int bottom, int right) {
  gfx::Insets insets(top, left, bottom, right);
  SetContentsResizingStrategy(insets, contents_resizing_strategy_.min_size());
}

void DevToolsWindow::SetContentsResizingStrategy(
    const gfx::Insets& insets, const gfx::Size& min_size) {
  DevToolsContentsResizingStrategy strategy(insets, min_size);
  if (contents_resizing_strategy_.Equals(strategy))
    return;

  contents_resizing_strategy_.CopyFrom(strategy);
  if (is_docked_) {
    // Update inspected window.
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->UpdateDevTools();
  }
}

void DevToolsWindow::InspectElementCompleted() {
  if (!inspect_element_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("DevTools.InspectElement",
        base::TimeTicks::Now() - inspect_element_start_time_);
    inspect_element_start_time_ = base::TimeTicks();
  }
}

void DevToolsWindow::MoveWindow(int x, int y) {
  if (!is_docked_) {
    gfx::Rect bounds = browser_->window()->GetBounds();
    bounds.Offset(x, y);
    browser_->window()->SetBounds(bounds);
  }
}

void DevToolsWindow::SetIsDockedAndShowImmediatelyForTest(bool is_docked) {
  DCHECK(!is_docked || can_dock_);
  if (load_state_ == kLoadCompleted) {
    SetIsDocked(is_docked);
  } else {
    is_docked_ = is_docked;
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Note that kIsDockedSet may be already set when can_dock_ is false.
    load_state_ = load_state_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    // Note that action_on_load_ will be performed after the load is actually
    // completed. For now, just show the window.
    Show(DevToolsToggleAction::Show());
    if (load_state_ == kLoadCompleted)
      LoadCompleted();
  }
  ignore_set_is_docked_ = true;
}

void DevToolsWindow::SetIsDocked(bool dock_requested) {
  if (ignore_set_is_docked_)
    return;

  DCHECK(can_dock_ || !dock_requested);
  if (!can_dock_)
    dock_requested = false;

  bool was_docked = is_docked_;
  is_docked_ = dock_requested;

  if (load_state_ != kLoadCompleted) {
    // This is a first time call we waited for to initialize.
    load_state_ = load_state_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    if (load_state_ == kLoadCompleted)
      LoadCompleted();
    return;
  }

  if (dock_requested == was_docked)
    return;

  if (dock_requested && !was_docked) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    tab_strip_model->DetachWebContentsAt(
        tab_strip_model->GetIndexOfWebContents(web_contents_));
    browser_ = NULL;
  } else if (!dock_requested && was_docked) {
    // Update inspected window to hide split and reset it.
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->UpdateDevTools();
  }

  Show(DevToolsToggleAction::Show());
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->OpenURL(params);
  } else {
    chrome::HostDesktopType host_desktop_type;
    if (browser_) {
      host_desktop_type = browser_->host_desktop_type();
    } else {
      // There should always be a browser when there are no inspected web
      // contents.
      NOTREACHED();
      host_desktop_type = chrome::GetActiveDesktop();
    }

    const BrowserList* browser_list =
        BrowserList::GetInstance(host_desktop_type);
    for (BrowserList::const_iterator it = browser_list->begin();
         it != browser_list->end(); ++it) {
      if ((*it)->type() == Browser::TYPE_TABBED) {
        (*it)->OpenURL(params);
        break;
      }
    }
  }
}

void DevToolsWindow::SaveToFile(const std::string& url,
                                const std::string& content,
                                bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::Bind(&DevToolsWindow::FileSavedAs,
                                weak_factory_.GetWeakPtr(), url),
                     base::Bind(&DevToolsWindow::CanceledFileSaveAs,
                                weak_factory_.GetWeakPtr(), url));
}

void DevToolsWindow::AppendToFile(const std::string& url,
                                  const std::string& content) {
  file_helper_->Append(url, content,
                       base::Bind(&DevToolsWindow::AppendedTo,
                                  weak_factory_.GetWeakPtr(), url));
}

void DevToolsWindow::RequestFileSystems() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RequestFileSystems(base::Bind(
      &DevToolsWindow::FileSystemsLoaded, weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::AddFileSystem() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->AddFileSystem(
      base::Bind(&DevToolsWindow::FileSystemAdded, weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsWindow::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RemoveFileSystem(file_system_path);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.fileSystemRemoved",
                     &file_system_path_value, NULL, NULL);
}

void DevToolsWindow::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->UpgradeDraggedFileSystemPermissions(
      file_system_url,
      base::Bind(&DevToolsWindow::FileSystemAdded, weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsWindow::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::IndexPath(int request_id,
                               const std::string& file_system_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    IndexingDone(request_id, file_system_path);
    return;
  }
  indexing_jobs_[request_id] =
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>(
          file_system_indexer_->IndexPath(
              file_system_path,
              Bind(&DevToolsWindow::IndexingTotalWorkCalculated,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsWindow::IndexingWorked,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsWindow::IndexingDone,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path)));
}

void DevToolsWindow::StopIndexing(int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  IndexingJobsMap::iterator it = indexing_jobs_.find(request_id);
  if (it == indexing_jobs_.end())
    return;
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsWindow::SearchInPath(int request_id,
                                  const std::string& file_system_path,
                                  const std::string& query) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    SearchCompleted(request_id, file_system_path, std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(file_system_path,
                                     query,
                                     Bind(&DevToolsWindow::SearchCompleted,
                                          weak_factory_.GetWeakPtr(),
                                          request_id,
                                          file_system_path));
}

void DevToolsWindow::ZoomIn() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_IN);
}

void DevToolsWindow::ZoomOut() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_OUT);
}

void DevToolsWindow::ResetZoom() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_RESET);
}

void DevToolsWindow::FileSavedAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.savedURL", &url_value, NULL, NULL);
}

void DevToolsWindow::CanceledFileSaveAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.canceledSaveURL",
                     &url_value, NULL, NULL);
}

void DevToolsWindow::AppendedTo(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.appendedToURL", &url_value, NULL,
                     NULL);
}

void DevToolsWindow::FileSystemsLoaded(
    const std::vector<DevToolsFileHelper::FileSystem>& file_systems) {
  base::ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i)
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  CallClientFunction("InspectorFrontendAPI.fileSystemsLoaded",
                     &file_systems_value, NULL, NULL);
}

void DevToolsWindow::FileSystemAdded(
    const DevToolsFileHelper::FileSystem& file_system) {
  scoped_ptr<base::StringValue> error_string_value(
      new base::StringValue(std::string()));
  scoped_ptr<base::DictionaryValue> file_system_value;
  if (!file_system.file_system_path.empty())
    file_system_value.reset(CreateFileSystemValue(file_system));
  CallClientFunction("InspectorFrontendAPI.fileSystemAdded",
                     error_string_value.get(), file_system_value.get(), NULL);
}

void DevToolsWindow::IndexingTotalWorkCalculated(
    int request_id,
    const std::string& file_system_path,
    int total_work) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue total_work_value(total_work);
  CallClientFunction("InspectorFrontendAPI.indexingTotalWorkCalculated",
                     &request_id_value, &file_system_path_value,
                     &total_work_value);
}

void DevToolsWindow::IndexingWorked(int request_id,
                                    const std::string& file_system_path,
                                    int worked) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue worked_value(worked);
  CallClientFunction("InspectorFrontendAPI.indexingWorked", &request_id_value,
                     &file_system_path_value, &worked_value);
}

void DevToolsWindow::IndexingDone(int request_id,
                                  const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.indexingDone", &request_id_value,
                     &file_system_path_value, NULL);
}

void DevToolsWindow::SearchCompleted(
    int request_id,
    const std::string& file_system_path,
    const std::vector<std::string>& file_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ListValue file_paths_value;
  for (std::vector<std::string>::const_iterator it(file_paths.begin());
       it != file_paths.end(); ++it) {
    file_paths_value.AppendString(*it);
  }
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.searchCompleted", &request_id_value,
                     &file_system_path_value, &file_paths_value);
}

void DevToolsWindow::ShowDevToolsConfirmInfoBar(
    const base::string16& message,
    const InfoBarCallback& callback) {
  DevToolsConfirmInfoBarDelegate::Create(
      is_docked_ ?
          InfoBarService::FromWebContents(GetInspectedWebContents()) :
          InfoBarService::FromWebContents(web_contents_),
      callback, message);
}

void DevToolsWindow::CreateDevToolsBrowser() {
  std::string wp_key = GetDevToolsWindowPlacementPrefKey();
  PrefService* prefs = profile_->GetPrefs();
  const base::DictionaryValue* wp_pref = prefs->GetDictionary(wp_key.c_str());
  if (!wp_pref || wp_pref->empty()) {
    DictionaryPrefUpdate update(prefs, wp_key.c_str());
    base::DictionaryValue* defaults = update.Get();
    defaults->SetInteger("left", 100);
    defaults->SetInteger("top", 100);
    defaults->SetInteger("right", 740);
    defaults->SetInteger("bottom", 740);
    defaults->SetBoolean("maximized", false);
    defaults->SetBoolean("always_on_top", false);
  }

  browser_ = new Browser(Browser::CreateParams::CreateForDevTools(
      profile_,
      chrome::GetHostDesktopTypeForNativeView(
          web_contents_->GetView()->GetNativeView())));
  browser_->tab_strip_model()->AddWebContents(
      web_contents_, -1, content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      TabStripModel::ADD_ACTIVE);
  GetRenderViewHost()->SyncRendererPrefs();
}

// static
bool DevToolsWindow::FindInspectedBrowserAndTabIndex(
    content::WebContents* inspected_web_contents, Browser** browser, int* tab) {
  if (!inspected_web_contents)
    return false;

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    int tab_index = it->tab_strip_model()->GetIndexOfWebContents(
        inspected_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      *browser = *it;
      *tab = tab_index;
      return true;
    }
  }
  return false;
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  Browser* browser = NULL;
  int tab;
  return FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                         &browser, &tab) ?
      browser->window() : NULL;
}

void DevToolsWindow::DoAction(const DevToolsToggleAction& action) {
  switch (action.type()) {
    case DevToolsToggleAction::kShowConsole:
      CallClientFunction("InspectorFrontendAPI.showConsole", NULL, NULL, NULL);
      break;

    case DevToolsToggleAction::kInspect:
      CallClientFunction("InspectorFrontendAPI.enterInspectElementMode", NULL,
                         NULL, NULL);
      break;

    case DevToolsToggleAction::kShow:
    case DevToolsToggleAction::kToggle:
      // Do nothing.
      break;

    case DevToolsToggleAction::kReveal: {
      const DevToolsToggleAction::RevealParams* params =
          action.params();
      CHECK(params);
      base::StringValue url_value(params->url);
      base::FundamentalValue line_value(static_cast<int>(params->line_number));
      base::FundamentalValue column_value(
          static_cast<int>(params->column_number));
      CallClientFunction("InspectorFrontendAPI.revealSourceLine",
                         &url_value,
                         &line_value,
                         &column_value);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void DevToolsWindow::UpdateTheme() {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  std::string command("InspectorFrontendAPI.setToolbarColors(\"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "\", \"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)) +
      "\")");
  web_contents_->GetMainFrame()->ExecuteJavaScript(base::ASCIIToUTF16(command));
}

void DevToolsWindow::AddDevToolsExtensionsToClient() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      base::FundamentalValue tabId(session_tab_helper->session_id().id());
      CallClientFunction("WebInspector.setInspectedTabId", &tabId, NULL, NULL);
    }
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  const ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      profile->GetOriginalProfile())->extension_service();
  if (!extension_service)
    return;
  const extensions::ExtensionSet* extensions = extension_service->extensions();

  base::ListValue results;
  for (extensions::ExtensionSet::const_iterator extension(extensions->begin());
       extension != extensions->end(); ++extension) {
    if (extensions::ManifestURL::GetDevToolsPage(extension->get()).is_empty())
      continue;
    base::DictionaryValue* extension_info = new base::DictionaryValue();
    extension_info->Set(
        "startPage",
        new base::StringValue(
            extensions::ManifestURL::GetDevToolsPage(extension->get()).spec()));
    extension_info->Set("name", new base::StringValue((*extension)->name()));
    extension_info->Set(
        "exposeExperimentalAPIs",
        new base::FundamentalValue((*extension)->HasAPIPermission(
            extensions::APIPermission::kExperimental)));
    results.Append(extension_info);
  }
  CallClientFunction("WebInspector.addExtensions", &results, NULL, NULL);
}

void DevToolsWindow::CallClientFunction(const std::string& function_name,
                                        const base::Value* arg1,
                                        const base::Value* arg2,
                                        const base::Value* arg3) {
  std::string params;
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(arg1, &json);
    params.append(json);
    if (arg2) {
      base::JSONWriter::Write(arg2, &json);
      params.append(", " + json);
      if (arg3) {
        base::JSONWriter::Write(arg3, &json);
        params.append(", " + json);
      }
    }
  }
  base::string16 javascript =
      base::ASCIIToUTF16(function_name + "(" + params + ");");
  web_contents_->GetMainFrame()->ExecuteJavaScript(javascript);
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(NULL);
}

content::WebContents* DevToolsWindow::GetInspectedWebContents() {
  return inspected_contents_observer_ ?
      inspected_contents_observer_->web_contents() : NULL;
}

void DevToolsWindow::DocumentOnLoadCompletedInMainFrame() {
  // We could be in kLoadCompleted state already if frontend reloads itself.
  if (load_state_ != kLoadCompleted) {
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Here we set kOnLoadFired.
    load_state_ = load_state_ == kIsDockedSet ? kLoadCompleted : kOnLoadFired;
  }
  if (load_state_ == kLoadCompleted)
    LoadCompleted();
}

void DevToolsWindow::LoadCompleted() {
  Show(action_on_load_);
  action_on_load_ = DevToolsToggleAction::NoOp();
  UpdateTheme();
  AddDevToolsExtensionsToClient();
  if (!load_completed_callback_.is_null()) {
    load_completed_callback_.Run();
    load_completed_callback_ = base::Closure();
  }
}

void DevToolsWindow::SetLoadCompletedCallback(const base::Closure& closure) {
  if (load_state_ == kLoadCompleted) {
    if (!closure.is_null())
      closure.Run();
    return;
  }
  load_completed_callback_ = closure;
}

void DevToolsWindow::SetWhitelistedShortcuts(
    const std::string& message) {
  event_forwarder_->SetWhitelistedShortcuts(message);
}

bool DevToolsWindow::ForwardKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return event_forwarder_->ForwardEvent(event);
}
