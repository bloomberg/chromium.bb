// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::DictionaryValue;
using blink::WebInputEvent;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WebContents;

namespace {

typedef std::vector<DevToolsWindow*> DevToolsWindows;
base::LazyInstance<DevToolsWindows>::Leaky g_instances =
    LAZY_INSTANCE_INITIALIZER;

static const char kKeyUpEventName[] = "keyup";
static const char kKeyDownEventName[] = "keydown";

bool FindInspectedBrowserAndTabIndex(
    WebContents* inspected_web_contents, Browser** browser, int* tab) {
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

// DevToolsToolboxDelegate ----------------------------------------------------

class DevToolsToolboxDelegate
    : public content::WebContentsObserver,
      public content::WebContentsDelegate {
 public:
  DevToolsToolboxDelegate(
      WebContents* toolbox_contents,
      DevToolsWindow::ObserverWithAccessor* web_contents_observer);
  virtual ~DevToolsToolboxDelegate();

  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

 private:
  BrowserWindow* GetInspectedBrowserWindow();
  DevToolsWindow::ObserverWithAccessor* inspected_contents_observer_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsToolboxDelegate);
};

DevToolsToolboxDelegate::DevToolsToolboxDelegate(
    WebContents* toolbox_contents,
    DevToolsWindow::ObserverWithAccessor* web_contents_observer)
    : WebContentsObserver(toolbox_contents),
      inspected_contents_observer_(web_contents_observer) {
}

DevToolsToolboxDelegate::~DevToolsToolboxDelegate() {
}

content::WebContents* DevToolsToolboxDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == web_contents());
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme))
    return NULL;
  content::NavigationController::LoadURLParams load_url_params(params.url);
  source->GetController().LoadURLWithParams(load_url_params);
  return source;
}

bool DevToolsToolboxDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
  return false;
}

void DevToolsToolboxDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windowsKeyCode == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return;
  }
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    window->HandleKeyboardEvent(event);
}

void DevToolsToolboxDelegate::WebContentsDestroyed() {
  delete this;
}

BrowserWindow* DevToolsToolboxDelegate::GetInspectedBrowserWindow() {
  WebContents* inspected_contents =
      inspected_contents_observer_->GetWebContents();
  if (!inspected_contents)
    return NULL;
  Browser* browser = NULL;
  int tab = 0;
  if (FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return browser->window();
  return NULL;
}

}  // namespace

// DevToolsEventForwarder -----------------------------------------------------

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
  devtools_window_->bindings_->CallClientFunction(
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

// DevToolsWindow::ObserverWithAccessor -------------------------------

DevToolsWindow::ObserverWithAccessor::ObserverWithAccessor(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

DevToolsWindow::ObserverWithAccessor::~ObserverWithAccessor() {
}

WebContents* DevToolsWindow::ObserverWithAccessor::GetWebContents() {
  return web_contents();
}

// DevToolsWindow -------------------------------------------------------------

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

DevToolsWindow::~DevToolsWindow() {
  life_stage_ = kClosing;

  UpdateBrowserWindow();
  UpdateBrowserToolbar();

  if (toolbox_web_contents_)
    delete toolbox_web_contents_;

  DevToolsWindows* instances = g_instances.Pointer();
  DevToolsWindows::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);

  if (!close_callback_.is_null()) {
    close_callback_.Run();
    close_callback_ = base::Closure();
  }
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
      true,
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
content::WebContents* DevToolsWindow::GetInTabWebContents(
    WebContents* inspected_web_contents,
    DevToolsContentsResizingStrategy* out_strategy) {
  DevToolsWindow* window = GetInstanceForInspectedWebContents(
      inspected_web_contents);
  if (!window || window->life_stage_ == kClosing)
    return NULL;

  // Not yet loaded window is treated as docked, but we should not present it
  // until we decided on docking.
  bool is_docked_set = window->life_stage_ == kLoadCompleted ||
      window->life_stage_ == kIsDockedSet;
  if (!is_docked_set)
    return NULL;

  // Undocked window should have toolbox web contents.
  if (!window->is_docked_ && !window->toolbox_web_contents_)
    return NULL;

  if (out_strategy)
    out_strategy->CopyFrom(window->contents_resizing_strategy_);

  return window->is_docked_ ? window->main_web_contents_ :
      window->toolbox_web_contents_;
}

// static
DevToolsWindow* DevToolsWindow::GetInstanceForInspectedWebContents(
    WebContents* inspected_web_contents) {
  if (!inspected_web_contents || g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->GetInspectedWebContents() == inspected_web_contents)
      return *it;
  }
  return NULL;
}

// static
bool DevToolsWindow::IsDevToolsWindow(content::WebContents* web_contents) {
  if (!web_contents || g_instances == NULL)
    return false;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->main_web_contents_ == web_contents ||
        (*it)->toolbox_web_contents_ == web_contents)
      return true;
  }
  return false;
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    DevToolsAgentHost* worker_agent) {
  DevToolsWindow* window = FindDevToolsWindow(worker_agent);
  if (!window) {
    window = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    window->bindings_->AttachTo(worker_agent);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
  return window;
}

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindowForWorker(
    Profile* profile) {
  content::RecordAction(base::UserMetricsAction("DevTools_InspectWorker"));
  return Create(profile, GURL(), NULL, true, false, false, "");
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents) {
  return ToggleDevToolsWindow(
      inspected_web_contents, true, DevToolsToggleAction::Show(), "");
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents,
    const DevToolsToggleAction& action) {
  return ToggleDevToolsWindow(inspected_web_contents, true, action, "");
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
      browser->tab_strip_model()->GetActiveWebContents(),
      action.type() == DevToolsToggleAction::kInspect,
      action, "");
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host);
  if (!window) {
    window = Create(profile, DevToolsUI::GetProxyURL(frontend_url), NULL,
                    false, true, false, "");
    window->bindings_->AttachTo(agent_host);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    content::WebContents* inspected_web_contents,
    bool force_open,
    const DevToolsToggleAction& action,
    const std::string& settings) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_web_contents));
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_web_contents->GetBrowserContext());
    content::RecordAction(
        base::UserMetricsAction("DevTools_InspectRenderer"));
    window = Create(
        profile, GURL(), inspected_web_contents, false, false, true, settings);
    window->bindings_->AttachTo(agent.get());
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
void DevToolsWindow::InspectElement(
    content::WebContents* inspected_web_contents,
    int x,
    int y) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_web_contents));
  agent->InspectElement(x, y);
  bool should_measure_time = FindDevToolsWindow(agent.get()) == NULL;
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  DevToolsWindow* window = OpenDevToolsWindow(inspected_web_contents);
  if (should_measure_time)
    window->inspect_element_start_time_ = start_time;
}

void DevToolsWindow::ScheduleShow(const DevToolsToggleAction& action) {
  if (life_stage_ == kLoadCompleted) {
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
  if (life_stage_ == kClosing)
    return;

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
    main_web_contents_->SetDelegate(this);

    TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
    tab_strip_model->ActivateTabAt(inspected_tab_index, true);

    inspected_window->UpdateDevTools();
    main_web_contents_->SetInitialFocus();
    inspected_window->Show();
    // On Aura, focusing once is not enough. Do it again.
    // Note that focusing only here but not before isn't enough either. We just
    // need to focus twice.
    main_web_contents_->SetInitialFocus();

    PrefsTabHelper::CreateForWebContents(main_web_contents_);
    main_web_contents_->GetRenderViewHost()->SyncRendererPrefs();

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
    main_web_contents_->SetInitialFocus();
  }
  if (toolbox_web_contents_)
    UpdateBrowserWindow();

  DoAction(action);
}

// static
bool DevToolsWindow::HandleBeforeUnload(WebContents* frontend_contents,
    bool proceed, bool* proceed_to_fire_unload) {
  DevToolsWindow* window = AsDevToolsWindow(frontend_contents);
  if (!window)
    return false;
  if (!window->intercepted_page_beforeunload_)
    return false;
  window->BeforeUnloadFired(frontend_contents, proceed,
      proceed_to_fire_unload);
  return true;
}

// static
bool DevToolsWindow::InterceptPageBeforeUnload(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window || window->intercepted_page_beforeunload_)
    return false;

  // Not yet loaded frontend will not handle beforeunload.
  if (window->life_stage_ != kLoadCompleted)
    return false;

  window->intercepted_page_beforeunload_ = true;
  // Handle case of devtools inspecting another devtools instance by passing
  // the call up to the inspecting devtools instance.
  if (!DevToolsWindow::InterceptPageBeforeUnload(window->main_web_contents_)) {
    window->main_web_contents_->DispatchBeforeUnload(false);
  }
  return true;
}

// static
bool DevToolsWindow::NeedsToInterceptBeforeUnload(
    WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
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
  WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow* window = AsDevToolsWindow(contents);
  if (!window)
    return false;
  return window->intercepted_page_beforeunload_;
}

// static
void DevToolsWindow::OnPageCloseCanceled(WebContents* contents) {
  DevToolsWindow *window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window)
    return;
  window->intercepted_page_beforeunload_ = false;
  // Propagate to devtools opened on devtools if any.
  DevToolsWindow::OnPageCloseCanceled(window->main_web_contents_);
}

DevToolsWindow::DevToolsWindow(Profile* profile,
                               const GURL& url,
                               content::WebContents* inspected_web_contents,
                               bool can_dock)
    : profile_(profile),
      main_web_contents_(
          WebContents::Create(WebContents::CreateParams(profile))),
      toolbox_web_contents_(NULL),
      bindings_(NULL),
      browser_(NULL),
      is_docked_(true),
      can_dock_(can_dock),
      // This initialization allows external front-end to work without changes.
      // We don't wait for docking call, but instead immediately show undocked.
      // Passing "dockSide=undocked" parameter ensures proper UI.
      life_stage_(can_dock ? kNotLoaded : kIsDockedSet),
      action_on_load_(DevToolsToggleAction::NoOp()),
      intercepted_page_beforeunload_(false) {
  // Set up delegate, so we get fully-functional window immediately.
  // It will not appear in UI though until |life_stage_ == kLoadCompleted|.
  main_web_contents_->SetDelegate(this);

  main_web_contents_->GetController().LoadURL(
      DevToolsUIBindings::ApplyThemeToURL(profile, url), content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  bindings_ = DevToolsUIBindings::ForWebContents(main_web_contents_);
  DCHECK(bindings_);

  // Bindings take ownership over devtools as its delegate.
  bindings_->SetDelegate(this);
  // DevTools uses chrome_page_zoom::Zoom(), so main_web_contents_ requires a
  // ZoomController.
  ZoomController::CreateForWebContents(main_web_contents_);
  ZoomController::FromWebContents(main_web_contents_)
      ->SetShowsNotificationBubble(false);

  g_instances.Get().push_back(this);

  // There is no inspected_web_contents in case of various workers.
  if (inspected_web_contents)
    inspected_contents_observer_.reset(
        new ObserverWithAccessor(inspected_web_contents));

  // Initialize docked page to be of the right size.
  if (can_dock_ && inspected_web_contents) {
    content::RenderWidgetHostView* inspected_view =
        inspected_web_contents->GetRenderWidgetHostView();
    if (inspected_view && main_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size = inspected_view->GetViewBounds().size();
      main_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
  }

  event_forwarder_.reset(new DevToolsEventForwarder(this));
}

// static
DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    const GURL& frontend_url,
    content::WebContents* inspected_web_contents,
    bool shared_worker_frontend,
    bool external_frontend,
    bool can_dock,
    const std::string& settings) {
  if (inspected_web_contents) {
    // Check for a place to dock.
    Browser* browser = NULL;
    int tab;
    if (!FindInspectedBrowserAndTabIndex(inspected_web_contents,
                                         &browser, &tab) ||
        browser->is_type_popup()) {
      can_dock = false;
    }
  }

  // Create WebContents with devtools.
  GURL url(GetDevToolsURL(profile, frontend_url,
                          shared_worker_frontend,
                          external_frontend,
                          can_dock, settings));
  return new DevToolsWindow(profile, url, inspected_web_contents, can_dock);
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    const GURL& base_url,
                                    bool shared_worker_frontend,
                                    bool external_frontend,
                                    bool can_dock,
                                    const std::string& settings) {
  // Compatibility errors are encoded with data urls, pass them
  // through with no decoration.
  if (base_url.SchemeIs("data"))
    return base_url;

  std::string frontend_url(
      base_url.is_empty() ? chrome::kChromeUIDevToolsURL : base_url.spec());
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&"));
  if (shared_worker_frontend)
    url_string += "&isSharedWorker=true";
  if (external_frontend)
    url_string += "&remoteFrontend=true";
  if (can_dock)
    url_string += "&can_dock=true";
  if (settings.size())
    url_string += "&settings=" + settings;
  return GURL(url_string);
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  if (!agent_host || g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->bindings_->IsAttachedTo(agent_host))
      return *it;
  }
  return NULL;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    content::WebContents* web_contents) {
  if (!web_contents || g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->main_web_contents_ == web_contents)
      return *it;
  }
  return NULL;
}

WebContents* DevToolsWindow::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == main_web_contents_);
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme)) {
    WebContents* inspected_web_contents = GetInspectedWebContents();
    return inspected_web_contents ?
        inspected_web_contents->OpenURL(params) : NULL;
  }

  bindings_->Reattach();

  content::NavigationController::LoadURLParams load_url_params(params.url);
  main_web_contents_->GetController().LoadURLWithParams(load_url_params);
  return main_web_contents_;
}

void DevToolsWindow::ActivateContents(WebContents* contents) {
  if (is_docked_) {
    WebContents* inspected_tab = GetInspectedWebContents();
    inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else {
    browser_->window()->Activate();
  }
}

void DevToolsWindow::AddNewContents(WebContents* source,
                                    WebContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture,
                                    bool* was_blocked) {
  if (new_contents == toolbox_web_contents_) {
    toolbox_web_contents_->SetDelegate(
        new DevToolsToolboxDelegate(toolbox_web_contents_,
                                    inspected_contents_observer_.get()));
    if (main_web_contents_->GetRenderWidgetHostView() &&
        toolbox_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size =
          main_web_contents_->GetRenderWidgetHostView()->GetViewBounds().size();
      toolbox_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
    UpdateBrowserWindow();
    return;
  }

  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, new_contents, disposition, initial_pos, user_gesture,
        was_blocked);
  }
}

void DevToolsWindow::WebContentsCreated(WebContents* source_contents,
                                        int opener_render_frame_id,
                                        const base::string16& frame_name,
                                        const GURL& target_url,
                                        WebContents* new_contents) {
  if (target_url.SchemeIs(content::kChromeDevToolsScheme) &&
      target_url.query().find("toolbox=true") != std::string::npos) {
    CHECK(can_dock_);
    toolbox_web_contents_ = new_contents;
  }
}

void DevToolsWindow::CloseContents(WebContents* source) {
  CHECK(is_docked_);
  life_stage_ = kClosing;
  UpdateBrowserWindow();
  // In case of docked main_web_contents_, we own it so delete here.
  // Embedding DevTools window will be deleted as a result of
  // DevToolsUIBindings destruction.
  delete main_web_contents_;
}

void DevToolsWindow::ContentsZoomChange(bool zoom_in) {
  DCHECK(is_docked_);
  chrome_page_zoom::Zoom(main_web_contents_,
      zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
}

void DevToolsWindow::BeforeUnloadFired(WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (!intercepted_page_beforeunload_) {
    // Docked devtools window closed directly.
    if (proceed)
      bindings_->Detach();
    *proceed_to_fire_unload = proceed;
  } else {
    // Inspected page is attempting to close.
    WebContents* inspected_web_contents = GetInspectedWebContents();
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
    WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    return inspected_window->PreHandleKeyboardEvent(event,
                                                    is_keyboard_shortcut);
  }
  return false;
}

void DevToolsWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windowsKeyCode == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return;
  }
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->HandleKeyboardEvent(event);
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager() {
  WebContents* inspected_web_contents = GetInspectedWebContents();
  return (inspected_web_contents && inspected_web_contents->GetDelegate()) ?
      inspected_web_contents->GetDelegate()->GetJavaScriptDialogManager() :
      content::WebContentsDelegate::GetJavaScriptDialogManager();
}

content::ColorChooser* DevToolsWindow::OpenColorChooser(
    WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void DevToolsWindow::RunFileChooser(WebContents* web_contents,
                                    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void DevToolsWindow::WebContentsFocused(WebContents* contents) {
  Browser* inspected_browser = NULL;
  int inspected_tab_index = -1;
  if (is_docked_ && FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                                    &inspected_browser,
                                                    &inspected_tab_index))
    inspected_browser->window()->WebContentsFocused(contents);
}

bool DevToolsWindow::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void DevToolsWindow::ActivateWindow() {
  if (is_docked_ && GetInspectedBrowserWindow())
    main_web_contents_->Focus();
  else if (!is_docked_ && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void DevToolsWindow::CloseWindow() {
  DCHECK(is_docked_);
  life_stage_ = kClosing;
  main_web_contents_->DispatchBeforeUnload(false);
}

void DevToolsWindow::SetInspectedPageBounds(const gfx::Rect& rect) {
  DevToolsContentsResizingStrategy strategy(rect);
  if (contents_resizing_strategy_.Equals(strategy))
    return;

  contents_resizing_strategy_.CopyFrom(strategy);
  UpdateBrowserWindow();
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

void DevToolsWindow::SetIsDocked(bool dock_requested) {
  if (life_stage_ == kClosing)
    return;

  DCHECK(can_dock_ || !dock_requested);
  if (!can_dock_)
    dock_requested = false;

  bool was_docked = is_docked_;
  is_docked_ = dock_requested;

  if (life_stage_ != kLoadCompleted) {
    // This is a first time call we waited for to initialize.
    life_stage_ = life_stage_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    if (life_stage_ == kLoadCompleted)
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
        tab_strip_model->GetIndexOfWebContents(main_web_contents_));
    browser_ = NULL;
  } else if (!dock_requested && was_docked) {
    UpdateBrowserWindow();
  }

  Show(DevToolsToggleAction::Show());
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  WebContents* inspected_web_contents = GetInspectedWebContents();
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

void DevToolsWindow::SetWhitelistedShortcuts(
    const std::string& message) {
  event_forwarder_->SetWhitelistedShortcuts(message);
}

void DevToolsWindow::InspectedContentsClosing() {
  intercepted_page_beforeunload_ = false;
  life_stage_ = kClosing;
  main_web_contents_->GetRenderViewHost()->ClosePage();
}

InfoBarService* DevToolsWindow::GetInfoBarService() {
  return is_docked_ ?
      InfoBarService::FromWebContents(GetInspectedWebContents()) :
      InfoBarService::FromWebContents(main_web_contents_);
}

void DevToolsWindow::RenderProcessGone() {
  // Docked DevToolsWindow owns its main_web_contents_ and must delete it.
  // Undocked main_web_contents_ are owned and handled by browser.
  // see crbug.com/369932
  if (is_docked_)
    CloseContents(main_web_contents_);
}

void DevToolsWindow::OnLoadCompleted() {
  // First seed inspected tab id for extension APIs.
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      base::FundamentalValue tabId(session_tab_helper->session_id().id());
      bindings_->CallClientFunction("WebInspector.setInspectedTabId",
          &tabId, NULL, NULL);
    }
  }

  if (life_stage_ == kClosing)
    return;

  // We could be in kLoadCompleted state already if frontend reloads itself.
  if (life_stage_ != kLoadCompleted) {
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Here we set kOnLoadFired.
    life_stage_ = life_stage_ == kIsDockedSet ? kLoadCompleted : kOnLoadFired;
  }
  if (life_stage_ == kLoadCompleted)
    LoadCompleted();
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
          main_web_contents_->GetNativeView())));
  browser_->tab_strip_model()->AddWebContents(
      main_web_contents_, -1, content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      TabStripModel::ADD_ACTIVE);
  main_web_contents_->GetRenderViewHost()->SyncRendererPrefs();
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
      bindings_->CallClientFunction(
          "InspectorFrontendAPI.showConsole", NULL, NULL, NULL);
      break;

    case DevToolsToggleAction::kInspect:
      bindings_->CallClientFunction(
          "InspectorFrontendAPI.enterInspectElementMode", NULL, NULL, NULL);
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
      bindings_->CallClientFunction("InspectorFrontendAPI.revealSourceLine",
                                    &url_value, &line_value, &column_value);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(NULL);
}

void DevToolsWindow::UpdateBrowserWindow() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
}

WebContents* DevToolsWindow::GetInspectedWebContents() {
  return inspected_contents_observer_ ?
      inspected_contents_observer_->GetWebContents() : NULL;
}

void DevToolsWindow::LoadCompleted() {
  Show(action_on_load_);
  action_on_load_ = DevToolsToggleAction::NoOp();
  if (!load_completed_callback_.is_null()) {
    load_completed_callback_.Run();
    load_completed_callback_ = base::Closure();
  }
}

void DevToolsWindow::SetLoadCompletedCallback(const base::Closure& closure) {
  if (life_stage_ == kLoadCompleted || life_stage_ == kClosing) {
    if (!closure.is_null())
      closure.Run();
    return;
  }
  load_completed_callback_ = closure;
}

bool DevToolsWindow::ForwardKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return event_forwarder_->ForwardEvent(event);
}
