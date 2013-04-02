// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"

typedef std::vector<DevToolsWindow*> DevToolsWindowList;
namespace {
base::LazyInstance<DevToolsWindowList>::Leaky
     g_instances = LAZY_INSTANCE_INITIALIZER;
}  // namespace

using base::Bind;
using content::DevToolsAgentHost;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::FileChooserParams;
using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderViewHost;
using content::WebContents;

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

const char kOldPrefBottom[] = "bottom";
const char kOldPrefRight[] = "right";

const char kPrefBottom[] = "dock_bottom";
const char kPrefRight[] = "dock_right";
const char kPrefUndocked[] = "undocked";

const char kDockSideBottom[] = "bottom";
const char kDockSideRight[] = "right";
const char kDockSideUndocked[] = "undocked";
const char kDockSideMinimized[] = "minimized";

// Minimal height of devtools pane or content pane when devtools are docked
// to the browser window.
const int kMinDevToolsHeight = 50;
const int kMinDevToolsWidth = 150;
const int kMinContentsSize = 50;
const int kMinimizedDevToolsHeight = 24;

class DevToolsWindow::InspectedWebContentsObserver
  : public content::WebContentsObserver {
 public:
  explicit InspectedWebContentsObserver(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
    }

  content::WebContents* Get() { return web_contents(); }
};

// static
std::string DevToolsWindow::GetDevToolsWindowPlacementPrefKey() {
  std::string wp_key;
  wp_key.append(prefs::kBrowserWindowPlacement);
  wp_key.append("_");
  wp_key.append(kDevToolsApp);
  return wp_key;
}

// static
void DevToolsWindow::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDevToolsOpenDocked,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDevToolsDockSide,
                               kDockSideBottom,
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kDevToolsEditedFiles,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kDevToolsFileSystemPaths,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterDictionaryPref(GetDevToolsWindowPlacementPrefKey().c_str(),
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
DevToolsWindow* DevToolsWindow::GetDockedInstanceForInspectedTab(
    WebContents* inspected_web_contents) {
  if (!inspected_web_contents)
    return NULL;

  if (!DevToolsAgentHost::HasFor(inspected_web_contents->GetRenderViewHost()))
    return NULL;
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetOrCreateFor(
      inspected_web_contents->GetRenderViewHost()));
  DevToolsWindow* window = FindDevToolsWindow(agent);
  return window && window->IsDocked() ? window : NULL;
}

// static
bool DevToolsWindow::IsDevToolsWindow(RenderViewHost* window_rvh) {
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
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        worker_agent,
        window->frontend_host_.get());
  }
  window->Show(DEVTOOLS_TOGGLE_ACTION_SHOW);
  return window;
}

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindowForWorker(
    Profile* profile) {
  return Create(profile, GURL(), NULL, DEVTOOLS_DOCK_SIDE_UNDOCKED, true);
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    RenderViewHost* inspected_rvh) {
  return ToggleDevToolsWindow(inspected_rvh, true,
                              DEVTOOLS_TOGGLE_ACTION_SHOW);
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    Browser* browser,
    DevToolsToggleAction action) {
  if (action == DEVTOOLS_TOGGLE_ACTION_TOGGLE && browser->is_devtools()) {
    browser->tab_strip_model()->CloseAllTabs();
    return NULL;
  }
  RenderViewHost* inspected_rvh =
      browser->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  return ToggleDevToolsWindow(inspected_rvh,
                       action == DEVTOOLS_TOGGLE_ACTION_INSPECT,
                       action);
}

void DevToolsWindow::InspectElement(RenderViewHost* inspected_rvh,
                                    int x,
                                    int y) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  agent->InspectElement(x, y);
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  OpenDevToolsWindow(inspected_rvh);
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host);
  if (!window) {
    window = Create(profile, DevToolsUI::GetProxyURL(frontend_url), NULL,
                    DEVTOOLS_DOCK_SIDE_UNDOCKED, false);
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host, window->frontend_host_.get());
  }
  window->Show(DEVTOOLS_TOGGLE_ACTION_SHOW);
}

DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    const GURL& frontend_url,
    RenderViewHost* inspected_rvh,
    DevToolsDockSide dock_side,
    bool shared_worker_frontend) {
  // Create WebContents with devtools.
  WebContents* web_contents =
      WebContents::Create(WebContents::CreateParams(profile));
  GURL url = GetDevToolsURL(profile, frontend_url, dock_side,
                            shared_worker_frontend);
  web_contents->GetController().LoadURL(url, content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  content::DevToolsClientHost::SetupDevToolsFrontendClient(render_view_host);

  if (url.path().find(chrome::kChromeUIDevToolsHostedPath) == 0) {
    // Only allow file scheme in embedded front-end by default.
    int process_id = render_view_host->GetProcess()->GetID();
    content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
        process_id, chrome::kFileScheme);
  }

  return new DevToolsWindow(web_contents, profile, frontend_url, inspected_rvh,
                            dock_side);
}

DevToolsWindow::DevToolsWindow(WebContents* web_contents,
                               Profile* profile,
                               const GURL& frontend_url,
                               RenderViewHost* inspected_rvh,
                               DevToolsDockSide dock_side)
    : profile_(profile),
      web_contents_(web_contents),
      browser_(NULL),
      dock_side_(dock_side),
      is_loaded_(false),
      action_on_load_(DEVTOOLS_TOGGLE_ACTION_SHOW),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      width_(-1),
      height_(-1),
      dock_side_before_minimized_(dock_side) {
  frontend_host_.reset(
      DevToolsClientHost::CreateDevToolsFrontendHost(web_contents, this));
  file_helper_.reset(new DevToolsFileHelper(web_contents, profile));

  g_instances.Get().push_back(this);
  // Wipe out page icon so that the default application icon is used.
  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  entry->GetFavicon().image = gfx::Image();
  entry->GetFavicon().valid = true;

  // Register on-load actions.
  registrar_.Add(
      this,
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&web_contents->GetController()));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&web_contents->GetController()));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));
  // There is no inspected_rvh in case of shared workers.
  if (inspected_rvh)
    inspected_contents_observer_.reset(new InspectedWebContentsObserver(
        WebContents::FromRenderViewHost(inspected_rvh)));
}

DevToolsWindow::~DevToolsWindow() {
  DevToolsWindowList& instances = g_instances.Get();
  DevToolsWindowList::iterator it = std::find(instances.begin(),
                                              instances.end(),
                                              this);
  DCHECK(it != instances.end());
  instances.erase(it);
}

content::WebContents* DevToolsWindow::GetInspectedWebContents() {
  if (!inspected_contents_observer_)
    return NULL;
  return inspected_contents_observer_->Get();
}

void DevToolsWindow::InspectedContentsClosing() {
  Hide();
}

void DevToolsWindow::Hide() {
  if (IsDocked()) {
    // Update dev tools to reflect removed dev tools window.
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->UpdateDevTools();
    // In case of docked web_contents_, we own it so delete here.
    delete web_contents_;

    delete this;
  } else {
    // First, initiate self-destruct to free all the registrars.
    // Then close all tabs. Browser will take care of deleting web_contents_
    // for us.
    Browser* browser = browser_;
    delete this;
    browser->tab_strip_model()->CloseAllTabs();
  }
}

void DevToolsWindow::Show(DevToolsToggleAction action) {
  if (IsDocked()) {
    Browser* inspected_browser;
    int inspected_tab_index;
    // Tell inspected browser to update splitter and switch to inspected panel.
    if (!IsInspectedBrowserPopupOrPanel() &&
        FindInspectedBrowserAndTabIndex(&inspected_browser,
                                        &inspected_tab_index)) {
      BrowserWindow* inspected_window = inspected_browser->window();
      web_contents_->SetDelegate(this);
      inspected_window->UpdateDevTools();
      web_contents_->GetView()->SetInitialFocus();
      inspected_window->Show();
      TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
      tab_strip_model->ActivateTabAt(inspected_tab_index, true);
      ScheduleAction(action);
      return;
    } else {
      // Sometimes we don't know where to dock. Stay undocked.
      dock_side_ = DEVTOOLS_DOCK_SIDE_UNDOCKED;
    }
  }

  // Avoid consecutive window switching if the devtools window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || action != DEVTOOLS_TOGGLE_ACTION_INSPECT;

  if (!browser_)
    CreateDevToolsBrowser();

  if (should_show_window) {
    browser_->window()->Show();
    web_contents_->GetView()->SetInitialFocus();
  }

  ScheduleAction(action);
}

DevToolsClientHost* DevToolsWindow::GetDevToolsClientHostForTest() {
  return frontend_host_.get();
}

int DevToolsWindow::GetWidth(int container_width) {
  if (width_ == -1) {
    width_ = profile_->GetPrefs()->
        GetInteger(prefs::kDevToolsVSplitLocation);
  }

  // By default, size devtools as 1/3 of the browser window.
  if (width_ == -1)
    width_ = container_width / 3;

  // Respect the minimum devtools width preset.
  width_ = std::max(kMinDevToolsWidth, width_);

  // But it should never compromise the content window size unless the entire
  // window is tiny.
  width_ = std::min(container_width - kMinContentsSize, width_);
  return width_;
}

int DevToolsWindow::GetHeight(int container_height) {
  if (height_ == -1) {
    height_ = profile_->GetPrefs()->
        GetInteger(prefs::kDevToolsHSplitLocation);
  }

  // By default, size devtools as 1/3 of the browser window.
  if (height_ == -1)
    height_ = container_height / 3;

  // Respect the minimum devtools width preset.
  height_ = std::max(kMinDevToolsHeight, height_);

  // But it should never compromise the content window size.
  height_ = std::min(container_height - kMinContentsSize, height_);
  return height_;
}

int DevToolsWindow::GetMinimumWidth() {
  return kMinDevToolsWidth;
}

int DevToolsWindow::GetMinimumHeight() {
  return kMinDevToolsHeight;
}

void DevToolsWindow::SetWidth(int width) {
  width_ = width;
  profile_->GetPrefs()->SetInteger(prefs::kDevToolsVSplitLocation, width);
}

void DevToolsWindow::SetHeight(int height) {
  height_ = height;
  profile_->GetPrefs()->SetInteger(prefs::kDevToolsHSplitLocation, height);
}

int DevToolsWindow::GetMinimizedHeight() {
  return kMinimizedDevToolsHeight;
}

RenderViewHost* DevToolsWindow::GetRenderViewHost() {
  return web_contents_->GetRenderViewHost();
}

void DevToolsWindow::CreateDevToolsBrowser() {
  std::string wp_key = GetDevToolsWindowPlacementPrefKey();
  PrefService* prefs = profile_->GetPrefs();
  const DictionaryValue* wp_pref = prefs->GetDictionary(wp_key.c_str());
  if (!wp_pref || wp_pref->empty()) {
    DictionaryPrefUpdate update(prefs, wp_key.c_str());
    DictionaryValue* defaults = update.Get();
    defaults->SetInteger("left", 100);
    defaults->SetInteger("top", 100);
    defaults->SetInteger("right", 740);
    defaults->SetInteger("bottom", 740);
    defaults->SetBoolean("maximized", false);
    defaults->SetBoolean("always_on_top", false);
  }

  chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeView(
          web_contents_->GetView()->GetNativeView());

  browser_ = new Browser(Browser::CreateParams::CreateForDevTools(
                             profile_, host_desktop_type));
  browser_->tab_strip_model()->AddWebContents(
      web_contents_, -1, content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      TabStripModel::ADD_ACTIVE);
}

bool DevToolsWindow::FindInspectedBrowserAndTabIndex(Browser** browser,
                                                     int* tab) {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
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
  return FindInspectedBrowserAndTabIndex(&browser, &tab) ?
      browser->window() : NULL;
}

bool DevToolsWindow::IsInspectedBrowserPopupOrPanel() {
  Browser* browser = NULL;
  int tab;
  if (!FindInspectedBrowserAndTabIndex(&browser, &tab))
    return false;

  return browser->is_type_popup() || browser->is_type_panel();
}

void DevToolsWindow::UpdateFrontendDockSide() {
  base::StringValue dock_side(SideToString(dock_side_));
  CallClientFunction("InspectorFrontendAPI.setDockSide", &dock_side);
  base::FundamentalValue docked(IsDocked());
  CallClientFunction("InspectorFrontendAPI.setAttachedWindow", &docked);
}


void DevToolsWindow::AddDevToolsExtensionsToClient() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      base::FundamentalValue tabId(session_tab_helper->session_id().id());
      CallClientFunction("WebInspector.setInspectedTabId", &tabId);
    }
  }
  ListValue results;
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  const ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      profile->GetOriginalProfile())->extension_service();
  if (!extension_service)
    return;

  const ExtensionSet* extensions = extension_service->extensions();

  for (ExtensionSet::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (extensions::ManifestURL::GetDevToolsPage(*extension).is_empty())
      continue;
    DictionaryValue* extension_info = new DictionaryValue();
    extension_info->Set("startPage", new StringValue(
        extensions::ManifestURL::GetDevToolsPage(*extension).spec()));
    extension_info->Set("name", new StringValue((*extension)->name()));
    bool allow_experimental = (*extension)->HasAPIPermission(
        extensions::APIPermission::kExperimental);
    extension_info->Set("exposeExperimentalAPIs",
        new base::FundamentalValue(allow_experimental));
    results.Append(extension_info);
  }
  CallClientFunction("WebInspector.addExtensions", &results);
}

WebContents* DevToolsWindow::OpenURLFromTab(WebContents* source,
                                            const OpenURLParams& params) {
  if (!params.url.SchemeIs(chrome::kChromeDevToolsScheme)) {
    content::WebContents* inspected_web_contents = GetInspectedWebContents();
    if (inspected_web_contents)
      return inspected_web_contents->OpenURL(params);
    return NULL;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  scoped_refptr<DevToolsAgentHost> agent_host(
      manager->GetDevToolsAgentHostFor(frontend_host_.get()));
  if (!agent_host)
    return NULL;
  manager->ClientHostClosing(frontend_host_.get());
  manager->RegisterDevToolsClientHostFor(agent_host, frontend_host_.get());

  chrome::NavigateParams nav_params(profile_, params.url, params.transition);
  FillNavigateParamsFromOpenURLParams(&nav_params, params);
  nav_params.source_contents = source;
  nav_params.tabstrip_add_types = TabStripModel::ADD_NONE;
  nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = true;
  chrome::Navigate(&nav_params);
  return nav_params.target_contents;
}

void DevToolsWindow::CallClientFunction(const std::string& function_name,
                                        const Value* arg1,
                                        const Value* arg2) {
  std::string params;
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(arg1, &json);
    params.append(json);
    if (arg2) {
      base::JSONWriter::Write(arg2, &json);
      params.append(", " + json);
    }
  }
  string16 javascript = ASCIIToUTF16(function_name + "(" + params + ");");
  web_contents_->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(string16(), javascript);
}

void DevToolsWindow::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP && !is_loaded_) {
    is_loaded_ = true;
    UpdateTheme();
    DoAction();
    AddDevToolsExtensionsToClient();
  } else if (type == chrome::NOTIFICATION_TAB_CLOSING) {
    if (content::Source<NavigationController>(source).ptr() ==
            &web_contents_->GetController()) {
      // This happens when browser closes all of its tabs as a result
      // of window.Close event.
      // Notify manager that this DevToolsClientHost no longer exists and
      // initiate self-destuct here.
      DevToolsManager::GetInstance()->ClientHostClosing(frontend_host_.get());
      UpdateBrowserToolbar();
      delete this;
    }
  } else if (type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED) {
    UpdateTheme();
  }
}

void DevToolsWindow::ScheduleAction(DevToolsToggleAction action) {
  action_on_load_ = action;
  if (is_loaded_)
    DoAction();
}

void DevToolsWindow::DoAction() {
  UpdateFrontendDockSide();
  switch (action_on_load_) {
    case DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE:
      CallClientFunction("InspectorFrontendAPI.showConsole", NULL);
      break;
    case DEVTOOLS_TOGGLE_ACTION_INSPECT:
      CallClientFunction("InspectorFrontendAPI.enterInspectElementMode", NULL);
    case DEVTOOLS_TOGGLE_ACTION_SHOW:
    case DEVTOOLS_TOGGLE_ACTION_TOGGLE:
      // Do nothing.
      break;
    default:
      NOTREACHED();
  }
  action_on_load_ = DEVTOOLS_TOGGLE_ACTION_SHOW;
}

std::string SkColorToRGBAString(SkColor color) {
  // We convert the alpha using DoubleToString because StringPrintf will use
  // locale specific formatters (e.g., use , instead of . in German).
  return base::StringPrintf("rgba(%d,%d,%d,%s)", SkColorGetR(color),
      SkColorGetG(color), SkColorGetB(color),
      base::DoubleToString(SkColorGetA(color) / 255.0).c_str());
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    const GURL& base_url,
                                    DevToolsDockSide dock_side,
                                    bool shared_worker_frontend) {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile);
  CHECK(tp);

  SkColor color_toolbar =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR);
  SkColor color_tab_text =
      tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool experiments_enabled =
      command_line.HasSwitch(switches::kEnableDevToolsExperiments);

  std::string frontend_url = base_url.is_empty() ?
      chrome::kChromeUIDevToolsURL : base_url.spec();
  std::string params_separator =
      frontend_url.find("?") == std::string::npos ? "?" : "&";
  std::string url_string = base::StringPrintf("%s%s"
      "dockSide=%s&toolbarColor=%s&textColor=%s%s%s",
      frontend_url.c_str(),
      params_separator.c_str(),
      SideToString(dock_side).c_str(),
      SkColorToRGBAString(color_toolbar).c_str(),
      SkColorToRGBAString(color_tab_text).c_str(),
      shared_worker_frontend ? "&isSharedWorker=true" : "",
      experiments_enabled ? "&experiments=true" : "");
  return GURL(url_string);
}

void DevToolsWindow::UpdateTheme() {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
  CHECK(tp);

  SkColor color_toolbar =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR);
  SkColor color_tab_text =
      tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  std::string command = base::StringPrintf(
      "InspectorFrontendAPI.setToolbarColors(\"%s\", \"%s\")",
      SkColorToRGBAString(color_toolbar).c_str(),
      SkColorToRGBAString(color_tab_text).c_str());
  web_contents_->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(command));
}

void DevToolsWindow::AddNewContents(WebContents* source,
                                    WebContents* new_contents,
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

bool DevToolsWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  if (IsDocked()) {
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      return inspected_window->PreHandleKeyboardEvent(
          event, is_keyboard_shortcut);
  }
  return false;
}

void DevToolsWindow::HandleKeyboardEvent(WebContents* source,
                                         const NativeWebKeyboardEvent& event) {
  if (IsDocked()) {
    if (event.windowsKeyCode == 0x08) {
      // Do not navigate back in history on Windows (http://crbug.com/74156).
      return;
    }
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->HandleKeyboardEvent(event);
  }
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    RenderViewHost* inspected_rvh,
    bool force_open,
    DevToolsToggleAction action) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  DevToolsManager* manager = DevToolsManager::GetInstance();
  DevToolsWindow* window = FindDevToolsWindow(agent);
  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_rvh->GetProcess()->GetBrowserContext());
    DevToolsDockSide dock_side = GetDockSideFromPrefs(profile);
    window = Create(profile, GURL(), inspected_rvh, dock_side, false);
    manager->RegisterDevToolsClientHostFor(agent, window->frontend_host_.get());
    do_open = true;
  }

  // Update toolbar to reflect DevTools changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it. If window is minimized, we maximize it.
  if (window->dock_side_ == DEVTOOLS_DOCK_SIDE_MINIMIZED)
    window->Restore();
  else if (!window->IsDocked() || do_open)
    window->Show(action);
  else
    window->CloseWindow();

  return window;
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  DevToolsWindowList& instances = g_instances.Get();
  for (DevToolsWindowList::iterator it = instances.begin();
       it != instances.end(); ++it) {
    if (manager->GetDevToolsAgentHostFor((*it)->frontend_host_.get()) ==
        agent_host)
      return *it;
  }
  return NULL;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(RenderViewHost* window_rvh) {
  if (g_instances == NULL)
    return NULL;
  DevToolsWindowList& instances = g_instances.Get();
  for (DevToolsWindowList::iterator it = instances.begin();
       it != instances.end(); ++it) {
    if ((*it)->web_contents_->GetRenderViewHost() == window_rvh)
      return *it;
  }
  return NULL;
}

void DevToolsWindow::ActivateWindow() {
  if (!IsDocked()) {
    if (!browser_->window()->IsActive()) {
      browser_->window()->Activate();
    }
  } else {
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      web_contents_->GetView()->Focus();
  }
}

void DevToolsWindow::ChangeAttachedWindowHeight(unsigned height) {
  if (dock_side_ != DEVTOOLS_DOCK_SIDE_BOTTOM)
    return;

  SetHeight(height);
  // Update inspected window to adjust heights.
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
}

void DevToolsWindow::CloseWindow() {
  DCHECK(IsDocked());
  DevToolsManager::GetInstance()->ClientHostClosing(frontend_host_.get());
  Hide();
}

void DevToolsWindow::MoveWindow(int x, int y) {
  if (!IsDocked()) {
    gfx::Rect bounds = browser_->window()->GetBounds();
    bounds.Offset(x, y);
    browser_->window()->SetBounds(bounds);
  }
}

void DevToolsWindow::SetDockSide(const std::string& side) {
  DevToolsDockSide requested_side = SideFromString(side);
  bool dock_requested = requested_side != DEVTOOLS_DOCK_SIDE_UNDOCKED;
  bool is_docked = IsDocked();

  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (dock_requested && (!inspected_web_contents ||
      !GetInspectedBrowserWindow() || IsInspectedBrowserPopupOrPanel())) {
      // Cannot dock, avoid window flashing due to close-reopen cycle.
    return;
  }

  if (dock_side_ != DEVTOOLS_DOCK_SIDE_MINIMIZED &&
      requested_side == DEVTOOLS_DOCK_SIDE_MINIMIZED) {
    dock_side_before_minimized_ = dock_side_;
  }

  dock_side_ = requested_side;
  if (dock_requested) {
    if (!is_docked) {
      // Detach window from the external devtools browser. It will lead to
      // the browser object's close and delete. Remove observer first.
      TabStripModel* tab_strip_model = browser_->tab_strip_model();
      tab_strip_model->DetachWebContentsAt(
          tab_strip_model->GetIndexOfWebContents(web_contents_));
      browser_ = NULL;
    }
  } else if (is_docked) {
    // Update inspected window to hide split and reset it.
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->UpdateDevTools();
  }

  if (dock_side_ != DEVTOOLS_DOCK_SIDE_MINIMIZED) {
    std::string pref_value = kPrefBottom;
    switch (dock_side_) {
      case DEVTOOLS_DOCK_SIDE_UNDOCKED:
          pref_value = kPrefUndocked;
          break;
      case DEVTOOLS_DOCK_SIDE_RIGHT:
          pref_value = kPrefRight;
          break;
      case DEVTOOLS_DOCK_SIDE_BOTTOM:
          pref_value = kPrefBottom;
          break;
      case DEVTOOLS_DOCK_SIDE_MINIMIZED:
          // We don't persist minimized state.
          break;
    }
    profile_->GetPrefs()->SetString(prefs::kDevToolsDockSide, pref_value);
  }

  Show(DEVTOOLS_TOGGLE_ACTION_SHOW);
}

void DevToolsWindow::Restore() {
  if (dock_side_ == DEVTOOLS_DOCK_SIDE_MINIMIZED)
    SetDockSide(SideToString(dock_side_before_minimized_));
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  OpenURLParams params(GURL(url),
                       content::Referrer(),
                       NEW_FOREGROUND_TAB,
                       content::PAGE_TRANSITION_LINK,
                       false /* is_renderer_initiated */);
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
  file_helper_->Save(url, content, save_as, Bind(&DevToolsWindow::FileSavedAs,
                                                 weak_factory_.GetWeakPtr(),
                                                 url));
}

void DevToolsWindow::AppendToFile(const std::string& url,
                                  const std::string& content) {
  file_helper_->Append(url, content, Bind(&DevToolsWindow::AppendedTo,
                                          weak_factory_.GetWeakPtr(),
                                          url));
}

namespace {

DictionaryValue* CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  DictionaryValue* file_system_value = new DictionaryValue();
  file_system_value->SetString("fileSystemName", file_system.file_system_name);
  file_system_value->SetString("rootURL", file_system.root_url);
  file_system_value->SetString("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

} // namespace

void DevToolsWindow::RequestFileSystems() {
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  file_helper_->RequestFileSystems(
      Bind(&DevToolsWindow::FileSystemsLoaded, weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::AddFileSystem() {
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  file_helper_->AddFileSystem(
      Bind(&DevToolsWindow::FileSystemAdded, weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  file_helper_->RemoveFileSystem(file_system_path);
  StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.fileSystemRemoved",
                     &file_system_path_value);
}

void DevToolsWindow::FileSavedAs(const std::string& url) {
  StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.savedURL", &url_value);
}

void DevToolsWindow::AppendedTo(const std::string& url) {
  StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.appendedToURL", &url_value);
}

void DevToolsWindow::FileSystemsLoaded(
    const std::vector<DevToolsFileHelper::FileSystem>& file_systems) {
  ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i) {
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  }
  CallClientFunction("InspectorFrontendAPI.fileSystemsLoaded",
                     &file_systems_value);
}

void DevToolsWindow::FileSystemAdded(
    std::string error_string,
    const DevToolsFileHelper::FileSystem& file_system) {
  StringValue error_string_value(error_string);
  DictionaryValue* file_system_value = NULL;
  if (!file_system.file_system_path.empty())
    file_system_value = CreateFileSystemValue(file_system);
  CallClientFunction("InspectorFrontendAPI.fileSystemAdded",
                     &error_string_value,
                     file_system_value);
  if (file_system_value)
    delete file_system_value;
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents && inspected_web_contents->GetDelegate()) {
    return inspected_web_contents->GetDelegate()->
        GetJavaScriptDialogManager();
  }
  return content::WebContentsDelegate::GetJavaScriptDialogManager();
}

void DevToolsWindow::RunFileChooser(WebContents* web_contents,
                                    const FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void DevToolsWindow::WebContentsFocused(WebContents* contents) {
  Browser* inspected_browser = NULL;
  int inspected_tab_index = -1;

  if (IsDocked() && FindInspectedBrowserAndTabIndex(&inspected_browser,
                                                    &inspected_tab_index)) {
    inspected_browser->window()->WebContentsFocused(contents);
  }
}

void DevToolsWindow::UpdateBrowserToolbar() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents)
    return;
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(inspected_web_contents, false);
}

bool DevToolsWindow::IsDocked() {
  return dock_side_ != DEVTOOLS_DOCK_SIDE_UNDOCKED;
}

// static
DevToolsDockSide DevToolsWindow::GetDockSideFromPrefs(Profile* profile) {
  std::string dock_side =
      profile->GetPrefs()->GetString(prefs::kDevToolsDockSide);

  // Migrate prefs
  if (dock_side == kOldPrefBottom || dock_side == kOldPrefRight) {
    bool docked = profile->GetPrefs()->GetBoolean(prefs::kDevToolsOpenDocked);
    if (dock_side == kOldPrefBottom)
      return docked ? DEVTOOLS_DOCK_SIDE_BOTTOM : DEVTOOLS_DOCK_SIDE_UNDOCKED;
    else
      return docked ? DEVTOOLS_DOCK_SIDE_RIGHT : DEVTOOLS_DOCK_SIDE_UNDOCKED;
  }

  if (dock_side == kPrefUndocked)
    return DEVTOOLS_DOCK_SIDE_UNDOCKED;
  else if (dock_side == kPrefRight)
    return DEVTOOLS_DOCK_SIDE_RIGHT;
  // Default to docked to bottom
  return DEVTOOLS_DOCK_SIDE_BOTTOM;
}

// static
std::string DevToolsWindow::SideToString(DevToolsDockSide dock_side) {
  std::string dock_side_string;
  switch (dock_side) {
    case DEVTOOLS_DOCK_SIDE_UNDOCKED: return kDockSideUndocked;
    case DEVTOOLS_DOCK_SIDE_RIGHT: return kDockSideRight;
    case DEVTOOLS_DOCK_SIDE_BOTTOM: return kDockSideBottom;
    case DEVTOOLS_DOCK_SIDE_MINIMIZED: return kDockSideMinimized;
  }
  return kDockSideUndocked;
}

// static
DevToolsDockSide DevToolsWindow::SideFromString(
    const std::string& dock_side) {
  if (dock_side == kDockSideRight)
    return DEVTOOLS_DOCK_SIDE_RIGHT;
  if (dock_side == kDockSideBottom)
    return DEVTOOLS_DOCK_SIDE_BOTTOM;
  if (dock_side == kDockSideMinimized)
    return DEVTOOLS_DOCK_SIDE_MINIMIZED;
  return DEVTOOLS_DOCK_SIDE_UNDOCKED;
}
