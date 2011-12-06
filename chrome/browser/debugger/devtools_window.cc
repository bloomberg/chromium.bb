// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_file_util.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/load_notification_details.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/bindings_policy.h"
#include "grit/generated_resources.h"

typedef std::vector<DevToolsWindow*> DevToolsWindowList;
namespace {
base::LazyInstance<DevToolsWindowList,
                   base::LeakyLazyInstanceTraits<DevToolsWindowList> >
     g_instances = LAZY_INSTANCE_INITIALIZER;
}  // namespace

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsClientHost;
using content::DevToolsManager;

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

// static
void DevToolsWindow::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kDevToolsOpenDocked,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

// static
TabContentsWrapper* DevToolsWindow::GetDevToolsContents(
    TabContents* inspected_tab) {
  if (!inspected_tab)
    return NULL;

  if (!DevToolsAgentHostRegistry::HasDevToolsAgentHost(
      inspected_tab->render_view_host()))
    return NULL;
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      inspected_tab->render_view_host());
  DevToolsManager* manager = DevToolsManager::GetInstance();
  DevToolsClientHost* client_host = manager->GetDevToolsClientHostFor(agent);
  DevToolsWindow* window = AsDevToolsWindow(client_host);
  if (!window || !window->is_docked())
    return NULL;
  return window->tab_contents();
}

// static
bool DevToolsWindow::IsDevToolsWindow(RenderViewHost* window_rvh) {
  if (g_instances == NULL)
    return NULL;
  DevToolsWindowList& instances = g_instances.Get();
  for (DevToolsWindowList::iterator it = instances.begin();
       it != instances.end(); ++it) {
    if ((*it)->tab_contents_->render_view_host() == window_rvh)
      return true;
  }
  return false;
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    DevToolsAgentHost* worker_agent) {
  DevToolsWindow* window;
  DevToolsClientHost* client = content::DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(worker_agent);
  if (client) {
    window = AsDevToolsWindow(client);
    if (!window)
      return NULL;
  } else {
    window = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        worker_agent,
        window->frontend_host_);
  }
  window->Show(DEVTOOLS_TOGGLE_ACTION_NONE);
  return window;
}

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindowForWorker(
    Profile* profile) {
  return Create(profile, NULL, false, true);
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    RenderViewHost* inspected_rvh) {
  return ToggleDevToolsWindow(inspected_rvh, true,
                              DEVTOOLS_TOGGLE_ACTION_NONE);
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    RenderViewHost* inspected_rvh,
    DevToolsToggleAction action) {
  return ToggleDevToolsWindow(inspected_rvh, false, action);
}

void DevToolsWindow::InspectElement(RenderViewHost* inspected_rvh,
                                    int x,
                                    int y) {
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      inspected_rvh);
  DevToolsManager::GetInstance()->InspectElement(agent, x, y);
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  OpenDevToolsWindow(inspected_rvh);
}


DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    RenderViewHost* inspected_rvh,
    bool docked,
    bool shared_worker_frontend) {
  // Create TabContents with devtools.
  TabContentsWrapper* tab_contents =
      Browser::TabContentsFactory(profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_contents->render_view_host()->AllowBindings(
      content::BINDINGS_POLICY_WEB_UI);
  tab_contents->controller().LoadURL(
      GetDevToolsUrl(profile, docked, shared_worker_frontend),
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());
  return new DevToolsWindow(tab_contents, profile, inspected_rvh, docked);
}

DevToolsWindow::DevToolsWindow(TabContentsWrapper* tab_contents,
                               Profile* profile,
                               RenderViewHost* inspected_rvh,
                               bool docked)
    : profile_(profile),
      inspected_tab_(NULL),
      tab_contents_(tab_contents),
      browser_(NULL),
      docked_(docked),
      is_loaded_(false),
      action_on_load_(DEVTOOLS_TOGGLE_ACTION_NONE),
      frontend_host_(NULL) {
  frontend_host_ = DevToolsClientHost::CreateDevToolsFrontendHost(
      tab_contents->tab_contents(),
      this);
  g_instances.Get().push_back(this);
  // Wipe out page icon so that the default application icon is used.
  NavigationEntry* entry = tab_contents_->controller().GetActiveEntry();
  entry->favicon().set_bitmap(SkBitmap());
  entry->favicon().set_is_valid(true);

  // Register on-load actions.
  registrar_.Add(
      this,
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab_contents_->controller()));
  registrar_.Add(
      this,
      content::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&tab_contents_->controller()));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));
  // There is no inspected_rvh in case of shared workers.
  if (inspected_rvh) {
    TabContents* tab = inspected_rvh->delegate()->GetAsTabContents();
    if (tab)
      inspected_tab_ = TabContentsWrapper::GetCurrentWrapperForContents(tab);
  }
}

DevToolsWindow::~DevToolsWindow() {
  DevToolsWindowList& instances = g_instances.Get();
  DevToolsWindowList::iterator it = std::find(instances.begin(),
                                              instances.end(),
                                              this);
  DCHECK(it != instances.end());
  instances.erase(it);
}

void DevToolsWindow::InspectedTabClosing() {
  if (docked_) {
    // Update dev tools to reflect removed dev tools window.

    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->UpdateDevTools();
    // In case of docked tab_contents we own it, so delete here.
    delete tab_contents_;

    delete this;
  } else {
    // First, initiate self-destruct to free all the registrars.
    // Then close all tabs. Browser will take care of deleting tab_contents
    // for us.
    Browser* browser = browser_;
    delete this;
    browser->CloseAllTabs();
  }
}

void DevToolsWindow::TabReplaced(TabContents* new_tab) {
  TabContentsWrapper* new_tab_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(new_tab);
  DCHECK(new_tab_wrapper);
  if (!new_tab_wrapper)
      return;
  DCHECK_EQ(profile_, new_tab_wrapper->profile());
  inspected_tab_ = new_tab_wrapper;
}

void DevToolsWindow::Show(DevToolsToggleAction action) {
  if (docked_) {
    Browser* inspected_browser;
    int inspected_tab_index;
    // Tell inspected browser to update splitter and switch to inspected panel.
    if (!IsInspectedBrowserPopupOrPanel() &&
        FindInspectedBrowserAndTabIndex(&inspected_browser,
                                        &inspected_tab_index)) {
      BrowserWindow* inspected_window = inspected_browser->window();
      tab_contents_->tab_contents()->set_delegate(this);
      inspected_window->UpdateDevTools();
      tab_contents_->view()->SetInitialFocus();
      inspected_window->Show();
      TabStripModel* tabstrip_model = inspected_browser->tabstrip_model();
      tabstrip_model->ActivateTabAt(inspected_tab_index, true);
      ScheduleAction(action);
      return;
    } else {
      // Sometimes we don't know where to dock. Stay undocked.
      docked_ = false;
      UpdateFrontendAttachedState();
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
    tab_contents_->view()->SetInitialFocus();
  }

  ScheduleAction(action);
}

void DevToolsWindow::RequestSetDocked(bool docked) {
  if (docked_ == docked)
    return;

  if (!inspected_tab_)
    return;

  profile_->GetPrefs()->SetBoolean(prefs::kDevToolsOpenDocked, docked);

  if (docked && (!GetInspectedBrowserWindow() ||
                 IsInspectedBrowserPopupOrPanel())) {
    // Cannot dock, avoid window flashing due to close-reopen cycle.
    return;
  }
  docked_ = docked;

  if (docked) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tabstrip_model = browser_->tabstrip_model();
    tabstrip_model->DetachTabContentsAt(
        tabstrip_model->GetIndexOfTabContents(tab_contents_));
    browser_ = NULL;
  } else {
    // Update inspected window to hide split and reset it.
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window) {
      inspected_window->UpdateDevTools();
      inspected_window = NULL;
    }
  }
  Show(DEVTOOLS_TOGGLE_ACTION_NONE);
}

RenderViewHost* DevToolsWindow::GetRenderViewHost() {
  return tab_contents_->render_view_host();
}

void DevToolsWindow::CreateDevToolsBrowser() {
  // TODO(pfeldman): Make browser's getter for this key static.
  std::string wp_key;
  wp_key.append(prefs::kBrowserWindowPlacement);
  wp_key.append("_");
  wp_key.append(kDevToolsApp);

  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->FindPreference(wp_key.c_str())) {
    prefs->RegisterDictionaryPref(wp_key.c_str(), PrefService::UNSYNCABLE_PREF);
  }

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

  browser_ = Browser::CreateForDevTools(profile_);
  browser_->tabstrip_model()->AddTabContents(
      tab_contents_, -1, content::PAGE_TRANSITION_START_PAGE,
      TabStripModel::ADD_ACTIVE);
}

bool DevToolsWindow::FindInspectedBrowserAndTabIndex(Browser** browser,
                                                     int* tab) {
  if (!inspected_tab_)
    return false;

  const NavigationController& controller = inspected_tab_->controller();
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    int tab_index = (*it)->GetIndexOfController(&controller);
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

void DevToolsWindow::UpdateFrontendAttachedState() {
  tab_contents_->render_view_host()->ExecuteJavascriptInWebFrame(
      string16(),
      docked_ ? ASCIIToUTF16("WebInspector.setAttachedWindow(true);")
              : ASCIIToUTF16("WebInspector.setAttachedWindow(false);"));
}


void DevToolsWindow::AddDevToolsExtensionsToClient() {
  if (inspected_tab_) {
    base::FundamentalValue tabId(
        inspected_tab_->restore_tab_helper()->session_id().id());
    CallClientFunction(ASCIIToUTF16("WebInspector.setInspectedTabId"), tabId);
  }
  ListValue results;
  const ExtensionService* extension_service =
      tab_contents_->profile()->GetOriginalProfile()->GetExtensionService();
  if (!extension_service)
    return;

  const ExtensionList* extensions = extension_service->extensions();

  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if ((*extension)->devtools_url().is_empty())
      continue;
    DictionaryValue* extension_info = new DictionaryValue();
    extension_info->Set("startPage",
        new StringValue((*extension)->devtools_url().spec()));
    extension_info->Set("name", new StringValue((*extension)->name()));
    results.Append(extension_info);
  }
  CallClientFunction(ASCIIToUTF16("WebInspector.addExtensions"), results);
}

TabContents* DevToolsWindow::OpenURLFromTab(TabContents* source,
                                            const OpenURLParams& params) {
  if (inspected_tab_) {
    OpenURLParams forward_params = params;
    forward_params.disposition = NEW_FOREGROUND_TAB;
    forward_params.transition = content::PAGE_TRANSITION_LINK;
    return inspected_tab_->tab_contents()->OpenURL(forward_params);
  }
  return NULL;
}

void DevToolsWindow::CallClientFunction(const string16& function_name,
                                        const Value& arg) {
  std::string json;
  base::JSONWriter::Write(&arg, false, &json);
  string16 javascript = function_name + char16('(') + UTF8ToUTF16(json) +
      ASCIIToUTF16(");");
  tab_contents_->render_view_host()->
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
  } else if (type == content::NOTIFICATION_TAB_CLOSING) {
    if (content::Source<NavigationController>(source).ptr() ==
            &tab_contents_->controller()) {
      // This happens when browser closes all of its tabs as a result
      // of window.Close event.
      // Notify manager that this DevToolsClientHost no longer exists and
      // initiate self-destuct here.
      DevToolsManager::GetInstance()->ClientHostClosing(frontend_host_);
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
  UpdateFrontendAttachedState();
  // TODO: these messages should be pushed through the WebKit API instead.
  switch (action_on_load_) {
    case DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE:
      tab_contents_->render_view_host()->ExecuteJavascriptInWebFrame(
          string16(), ASCIIToUTF16("WebInspector.showConsole();"));
      break;
    case DEVTOOLS_TOGGLE_ACTION_INSPECT:
      tab_contents_->render_view_host()->ExecuteJavascriptInWebFrame(
          string16(), ASCIIToUTF16("WebInspector.toggleSearchingForNode();"));
    case DEVTOOLS_TOGGLE_ACTION_NONE:
      // Do nothing.
      break;
    default:
      NOTREACHED();
  }
  action_on_load_ = DEVTOOLS_TOGGLE_ACTION_NONE;
}

std::string SkColorToRGBAString(SkColor color) {
  // We convert the alpha using DoubleToString because StringPrintf will use
  // locale specific formatters (e.g., use , instead of . in German).
  return StringPrintf("rgba(%d,%d,%d,%s)", SkColorGetR(color),
      SkColorGetG(color), SkColorGetB(color),
      base::DoubleToString(SkColorGetA(color) / 255.0).c_str());
}

GURL DevToolsWindow::GetDevToolsUrl(Profile* profile, bool docked,
                                    bool shared_worker_frontend) {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile);
  CHECK(tp);

  SkColor color_toolbar =
      tp->GetColor(ThemeService::COLOR_TOOLBAR);
  SkColor color_tab_text =
      tp->GetColor(ThemeService::COLOR_BOOKMARK_TEXT);

  std::string url_string = StringPrintf(
      "%sdevtools.html?docked=%s&toolbarColor=%s&textColor=%s%s",
      chrome::kChromeUIDevToolsURL,
      docked ? "true" : "false",
      SkColorToRGBAString(color_toolbar).c_str(),
      SkColorToRGBAString(color_tab_text).c_str(),
      shared_worker_frontend ? "&isSharedWorker=true" : "");
  return GURL(url_string);
}

void DevToolsWindow::UpdateTheme() {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
  CHECK(tp);

  SkColor color_toolbar =
      tp->GetColor(ThemeService::COLOR_TOOLBAR);
  SkColor color_tab_text =
      tp->GetColor(ThemeService::COLOR_BOOKMARK_TEXT);
  std::string command = StringPrintf(
      "WebInspector.setToolbarColors(\"%s\", \"%s\")",
      SkColorToRGBAString(color_toolbar).c_str(),
      SkColorToRGBAString(color_tab_text).c_str());
  tab_contents_->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(command));
}

void DevToolsWindow::AddNewContents(TabContents* source,
                                    TabContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture) {
  if (inspected_tab_) {
    inspected_tab_->tab_contents()->delegate()->AddNewContents(
        source, new_contents, disposition, initial_pos, user_gesture);
  }
}

bool DevToolsWindow::CanReloadContents(TabContents* source) const {
  return false;
}

bool DevToolsWindow::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  if (docked_) {
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      return inspected_window->PreHandleKeyboardEvent(
          event, is_keyboard_shortcut);
  }
  return false;
}

void DevToolsWindow::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (docked_) {
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
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      inspected_rvh);
  DevToolsManager* manager = DevToolsManager::GetInstance();
  DevToolsClientHost* host = manager->GetDevToolsClientHostFor(agent);
  DevToolsWindow* window = AsDevToolsWindow(host);
  if (host != NULL && window == NULL) {
    // Break remote debugging / extension debugging session.
    manager->UnregisterDevToolsClientHostFor(agent);
  }

  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_rvh->process()->GetBrowserContext());
    bool docked = profile->GetPrefs()->GetBoolean(prefs::kDevToolsOpenDocked);
    window = Create(profile, inspected_rvh, docked, false);
    manager->RegisterDevToolsClientHostFor(agent, window->frontend_host_);
    do_open = true;
  }

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it.
  if (!window->is_docked() || do_open)
    window->Show(action);
  else
    manager->UnregisterDevToolsClientHostFor(agent);

  return window;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    DevToolsClientHost* client_host) {
  if (!client_host || g_instances == NULL)
    return NULL;
  DevToolsWindowList& instances = g_instances.Get();
  for (DevToolsWindowList::iterator it = instances.begin();
       it != instances.end(); ++it) {
    if ((*it)->frontend_host_ == client_host)
      return *it;
  }
  return NULL;
}

void DevToolsWindow::ActivateWindow() {
  if (!docked_) {
    if (!browser_->window()->IsActive()) {
      browser_->window()->Activate();
    }
  } else {
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      tab_contents_->view()->Focus();
  }
}

void DevToolsWindow::CloseWindow() {
  DCHECK(docked_);
  DevToolsManager::GetInstance()->ClientHostClosing(frontend_host_);
  InspectedTabClosing();
}

void DevToolsWindow::MoveWindow(int x, int y) {
  if (!docked_) {
    gfx::Rect bounds = browser_->window()->GetBounds();
    bounds.Offset(x, y);
    browser_->window()->SetBounds(bounds);
  }
}

void DevToolsWindow::DockWindow() {
  RequestSetDocked(true);
}

void DevToolsWindow::UndockWindow() {
  RequestSetDocked(false);
}

void DevToolsWindow::SaveToFile(const std::string& suggested_file_name,
                                const std::string& content) {
  DevToolsFileUtil::SaveAs(tab_contents_->profile(),
                           suggested_file_name,
                           content);
}

content::JavaScriptDialogCreator* DevToolsWindow::GetJavaScriptDialogCreator() {
  if (inspected_tab_ && inspected_tab_->tab_contents()->delegate()) {
    return inspected_tab_->tab_contents()->delegate()->
        GetJavaScriptDialogCreator();
  }
  return TabContentsDelegate::GetJavaScriptDialogCreator();
}
