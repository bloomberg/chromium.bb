// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/debug/debugging_flags.h"
#include "base/debug/profiler.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

#if defined(USE_ASH)
#include "ash/accelerators/accelerator_commands.h"
#include "chrome/browser/ui/ash/ash_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser_commands_chromeos.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/events/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

using content::NavigationEntry;
using content::NavigationController;
using content::WebContents;

namespace {

enum WindowState {
  // Not in fullscreen mode.
  WINDOW_STATE_NOT_FULLSCREEN,

  // Fullscreen mode, occupying the whole screen.
  WINDOW_STATE_FULLSCREEN,

  // Fullscreen mode for metro snap, occupying the full height and 20% of
  // the screen width.
  WINDOW_STATE_METRO_SNAP,
};

// Returns |true| if entry has an internal chrome:// URL, |false| otherwise.
bool HasInternalURL(const NavigationEntry* entry) {
  if (!entry)
    return false;

  // Check the |virtual_url()| first. This catches regular chrome:// URLs
  // including URLs that were rewritten (such as chrome://bookmarks).
  if (entry->GetVirtualURL().SchemeIs(content::kChromeUIScheme))
    return true;

  // If the |virtual_url()| isn't a chrome:// URL, check if it's actually
  // view-source: of a chrome:// URL.
  if (entry->GetVirtualURL().SchemeIs(content::kViewSourceScheme))
    return entry->GetURL().SchemeIs(content::kChromeUIScheme);

  return false;
}

}  // namespace

namespace chrome {

///////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, public:

BrowserCommandController::BrowserCommandController(Browser* browser)
    : browser_(browser),
      command_updater_(this),
      block_command_execution_(false),
      last_blocked_command_id_(-1),
      last_blocked_command_disposition_(CURRENT_TAB) {
  browser_->tab_strip_model()->AddObserver(this);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(
        prefs::kAllowFileSelectionDialogs,
        base::Bind(
            &BrowserCommandController::UpdateCommandsForFileSelectionDialogs,
            base::Unretained(this)));
  }

  profile_pref_registrar_.Init(profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDevToolsDisabled,
      base::Bind(&BrowserCommandController::UpdateCommandsForDevTools,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::Bind(&BrowserCommandController::UpdateCommandsForBookmarkEditing,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::Bind(&BrowserCommandController::UpdateCommandsForBookmarkBar,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(
          &BrowserCommandController::UpdateCommandsForIncognitoAvailability,
          base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kPrintingEnabled,
      base::Bind(&BrowserCommandController::UpdatePrintingState,
                 base::Unretained(this)));
#if !defined(OS_MACOSX)
  profile_pref_registrar_.Add(
      prefs::kFullscreenAllowed,
      base::Bind(&BrowserCommandController::UpdateCommandsForFullscreenMode,
                 base::Unretained(this)));
#endif
  pref_signin_allowed_.Init(
      prefs::kSigninAllowed,
      profile()->GetOriginalProfile()->GetPrefs(),
      base::Bind(&BrowserCommandController::OnSigninAllowedPrefChange,
                 base::Unretained(this)));

  InitCommandState();

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service) {
    tab_restore_service->AddObserver(this);
    TabRestoreServiceChanged(tab_restore_service);
  }
}

BrowserCommandController::~BrowserCommandController() {
  // TabRestoreService may have been shutdown by the time we get here. Don't
  // trigger creating it.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfileIfExisting(profile());
  if (tab_restore_service)
    tab_restore_service->RemoveObserver(this);
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool BrowserCommandController::IsReservedCommandOrKey(
    int command_id,
    const content::NativeWebKeyboardEvent& event) {
  // In Apps mode, no keys are reserved.
  if (browser_->is_app())
    return false;

#if defined(OS_CHROMEOS)
  // On Chrome OS, the top row of keys are mapped to browser actions like
  // back/forward or refresh. We don't want web pages to be able to change the
  // behavior of these keys.  Ash handles F4 and up; this leaves us needing to
  // reserve browser back/forward and refresh here.
  ui::KeyboardCode key_code =
    static_cast<ui::KeyboardCode>(event.windowsKeyCode);
  if ((key_code == ui::VKEY_BROWSER_BACK && command_id == IDC_BACK) ||
      (key_code == ui::VKEY_BROWSER_FORWARD && command_id == IDC_FORWARD) ||
      (key_code == ui::VKEY_BROWSER_REFRESH && command_id == IDC_RELOAD)) {
    return true;
  }
#endif

  if (window()->IsFullscreen() && command_id == IDC_FULLSCREEN)
    return true;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // If this key was registered by the user as a content editing hotkey, then
  // it is not reserved.
  ui::TextEditKeyBindingsDelegateAuraLinux* delegate =
      ui::GetTextEditKeyBindingsDelegate();
  if (delegate && event.os_event && delegate->MatchEvent(*event.os_event, NULL))
    return false;
#endif

  return command_id == IDC_CLOSE_TAB ||
         command_id == IDC_CLOSE_WINDOW ||
         command_id == IDC_NEW_INCOGNITO_WINDOW ||
         command_id == IDC_NEW_TAB ||
         command_id == IDC_NEW_WINDOW ||
         command_id == IDC_RESTORE_TAB ||
         command_id == IDC_SELECT_NEXT_TAB ||
         command_id == IDC_SELECT_PREVIOUS_TAB ||
         command_id == IDC_EXIT;
}

void BrowserCommandController::SetBlockCommandExecution(bool block) {
  block_command_execution_ = block;
  if (block) {
    last_blocked_command_id_ = -1;
    last_blocked_command_disposition_ = CURRENT_TAB;
  }
}

int BrowserCommandController::GetLastBlockedCommand(
    WindowOpenDisposition* disposition) {
  if (disposition)
    *disposition = last_blocked_command_disposition_;
  return last_blocked_command_id_;
}

void BrowserCommandController::TabStateChanged() {
  UpdateCommandsForTabState();
}

void BrowserCommandController::ZoomStateChanged() {
  UpdateCommandsForZoomState();
}

void BrowserCommandController::ContentRestrictionsChanged() {
  UpdateCommandsForContentRestrictionState();
}

void BrowserCommandController::FullscreenStateChanged() {
  UpdateCommandsForFullscreenMode();
}

void BrowserCommandController::PrintingStateChanged() {
  UpdatePrintingState();
}

void BrowserCommandController::LoadingStateChanged(bool is_loading,
                                                   bool force) {
  UpdateReloadStopState(is_loading, force);
}

void BrowserCommandController::ExtensionStateChanged() {
  // Extensions may disable the bookmark editing commands.
  UpdateCommandsForBookmarkEditing();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, CommandUpdaterDelegate implementation:

void BrowserCommandController::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  // No commands are enabled if there is not yet any selected tab.
  // TODO(pkasting): It seems like we should not need this, because either
  // most/all commands should not have been enabled yet anyway or the ones that
  // are enabled should be global, or safe themselves against having no selected
  // tab.  However, Ben says he tried removing this before and got lots of
  // crashes, e.g. from Windows sending WM_COMMANDs at random times during
  // window construction.  This probably could use closer examination someday.
  if (browser_->tab_strip_model()->active_index() == TabStripModel::kNoTab)
    return;

  DCHECK(command_updater_.IsCommandEnabled(id)) << "Invalid/disabled command "
                                                << id;

  // If command execution is blocked then just record the command and return.
  if (block_command_execution_) {
    // We actually only allow no more than one blocked command, otherwise some
    // commands maybe lost.
    DCHECK_EQ(last_blocked_command_id_, -1);
    last_blocked_command_id_ = id;
    last_blocked_command_disposition_ = disposition;
    return;
  }

  // The order of commands in this switch statement must match the function
  // declaration order in browser.h!
  switch (id) {
    // Navigation commands
    case IDC_BACK:
      GoBack(browser_, disposition);
      break;
    case IDC_FORWARD:
      GoForward(browser_, disposition);
      break;
    case IDC_RELOAD:
      Reload(browser_, disposition);
      break;
    case IDC_RELOAD_CLEARING_CACHE:
      ClearCache(browser_);
      // FALL THROUGH
    case IDC_RELOAD_IGNORING_CACHE:
      ReloadIgnoringCache(browser_, disposition);
      break;
    case IDC_HOME:
      Home(browser_, disposition);
      break;
    case IDC_OPEN_CURRENT_URL:
      OpenCurrentURL(browser_);
      break;
    case IDC_STOP:
      Stop(browser_);
      break;

     // Window management commands
    case IDC_NEW_WINDOW:
      NewWindow(browser_);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      NewIncognitoWindow(browser_);
      break;
    case IDC_CLOSE_WINDOW:
      content::RecordAction(base::UserMetricsAction("CloseWindowByKey"));
      CloseWindow(browser_);
      break;
    case IDC_NEW_TAB:
      NewTab(browser_);
      break;
    case IDC_CLOSE_TAB:
      content::RecordAction(base::UserMetricsAction("CloseTabByKey"));
      CloseTab(browser_);
      break;
    case IDC_SELECT_NEXT_TAB:
      content::RecordAction(base::UserMetricsAction("Accel_SelectNextTab"));
      SelectNextTab(browser_);
      break;
    case IDC_SELECT_PREVIOUS_TAB:
      content::RecordAction(
          base::UserMetricsAction("Accel_SelectPreviousTab"));
      SelectPreviousTab(browser_);
      break;
    case IDC_MOVE_TAB_NEXT:
      MoveTabNext(browser_);
      break;
    case IDC_MOVE_TAB_PREVIOUS:
      MoveTabPrevious(browser_);
      break;
    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7:
      SelectNumberedTab(browser_, id - IDC_SELECT_TAB_0);
      break;
    case IDC_SELECT_LAST_TAB:
      SelectLastTab(browser_);
      break;
    case IDC_DUPLICATE_TAB:
      DuplicateTab(browser_);
      break;
    case IDC_RESTORE_TAB:
      RestoreTab(browser_);
      break;
    case IDC_SHOW_AS_TAB:
      ConvertPopupToTabbedBrowser(browser_);
      break;
    case IDC_FULLSCREEN:
#if defined(OS_MACOSX)
      chrome::ToggleFullscreenWithToolbarOrFallback(browser_);
#else
      chrome::ToggleFullscreenMode(browser_);
#endif
      break;

#if defined(OS_CHROMEOS)
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3:
      ExecuteVisitDesktopCommand(id, browser_->window()->GetNativeWindow());
      break;
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    case IDC_USE_SYSTEM_TITLE_BAR: {
      PrefService* prefs = browser_->profile()->GetPrefs();
      prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                        !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
      break;
    }
#endif

#if defined(OS_MACOSX)
    case IDC_PRESENTATION_MODE:
      chrome::ToggleFullscreenMode(browser_);
      break;
    case IDC_TOGGLE_FULLSCREEN_TOOLBAR:
      chrome::ToggleFullscreenToolbar(browser_);
      break;
#endif
    case IDC_EXIT:
      Exit();
      break;

    // Page-related commands
    case IDC_SAVE_PAGE:
      SavePage(browser_);
      break;
    case IDC_BOOKMARK_PAGE:
      BookmarkCurrentPageAllowingExtensionOverrides(browser_);
      break;
    case IDC_BOOKMARK_ALL_TABS:
      BookmarkAllTabs(browser_);
      break;
    case IDC_VIEW_SOURCE:
      ViewSelectedSource(browser_);
      break;
    case IDC_EMAIL_PAGE_LOCATION:
      EmailPageLocation(browser_);
      break;
    case IDC_PRINT:
      Print(browser_);
      break;

#if defined(ENABLE_BASIC_PRINTING)
    case IDC_BASIC_PRINT:
      content::RecordAction(base::UserMetricsAction("Accel_Advanced_Print"));
      BasicPrint(browser_);
      break;
#endif  // ENABLE_BASIC_PRINTING

    case IDC_SAVE_CREDIT_CARD_FOR_PAGE:
      SaveCreditCard(browser_);
      break;
    case IDC_TRANSLATE_PAGE:
      Translate(browser_);
      break;
    case IDC_MANAGE_PASSWORDS_FOR_PAGE:
      ManagePasswordsForPage(browser_);
      break;

    // Page encoding commands
    case IDC_ENCODING_AUTO_DETECT:
      browser_->ToggleEncodingAutoDetect();
      break;
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GBK:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5:
    case IDC_ENCODING_KOREAN:
    case IDC_ENCODING_SHIFTJIS:
    case IDC_ENCODING_ISO2022JP:
    case IDC_ENCODING_EUCJP:
    case IDC_ENCODING_THAI:
    case IDC_ENCODING_ISO885915:
    case IDC_ENCODING_MACINTOSH:
    case IDC_ENCODING_ISO88592:
    case IDC_ENCODING_WINDOWS1250:
    case IDC_ENCODING_ISO88595:
    case IDC_ENCODING_WINDOWS1251:
    case IDC_ENCODING_KOI8R:
    case IDC_ENCODING_KOI8U:
    case IDC_ENCODING_IBM866:
    case IDC_ENCODING_ISO88597:
    case IDC_ENCODING_WINDOWS1253:
    case IDC_ENCODING_ISO88594:
    case IDC_ENCODING_ISO885913:
    case IDC_ENCODING_WINDOWS1257:
    case IDC_ENCODING_ISO88593:
    case IDC_ENCODING_ISO885910:
    case IDC_ENCODING_ISO885914:
    case IDC_ENCODING_ISO885916:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_ISO88598I:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258:
      browser_->OverrideEncoding(id);
      break;

    // Clipboard commands
    case IDC_CUT:
    case IDC_COPY:
    case IDC_PASTE:
      CutCopyPaste(browser_, id);
      break;

    // Find-in-page
    case IDC_FIND:
      Find(browser_);
      break;
    case IDC_FIND_NEXT:
      FindNext(browser_);
      break;
    case IDC_FIND_PREVIOUS:
      FindPrevious(browser_);
      break;

    // Zoom
    case IDC_ZOOM_PLUS:
      Zoom(browser_, content::PAGE_ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      Zoom(browser_, content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_MINUS:
      Zoom(browser_, content::PAGE_ZOOM_OUT);
      break;

    // Focus various bits of UI
    case IDC_FOCUS_TOOLBAR:
      content::RecordAction(base::UserMetricsAction("Accel_Focus_Toolbar"));
      FocusToolbar(browser_);
      break;
    case IDC_FOCUS_LOCATION:
      content::RecordAction(base::UserMetricsAction("Accel_Focus_Location"));
      FocusLocationBar(browser_);
      break;
    case IDC_FOCUS_SEARCH:
      content::RecordAction(base::UserMetricsAction("Accel_Focus_Search"));
      FocusSearch(browser_);
      break;
    case IDC_FOCUS_MENU_BAR:
      FocusAppMenu(browser_);
      break;
    case IDC_FOCUS_BOOKMARKS:
      content::RecordAction(
          base::UserMetricsAction("Accel_Focus_Bookmarks"));
      FocusBookmarksToolbar(browser_);
      break;
    case IDC_FOCUS_INFOBARS:
      FocusInfobars(browser_);
      break;
    case IDC_FOCUS_NEXT_PANE:
      FocusNextPane(browser_);
      break;
    case IDC_FOCUS_PREVIOUS_PANE:
      FocusPreviousPane(browser_);
      break;

    // Show various bits of UI
    case IDC_OPEN_FILE:
      browser_->OpenFile();
      break;
    case IDC_CREATE_SHORTCUTS:
      CreateApplicationShortcuts(browser_);
      break;
    case IDC_CREATE_HOSTED_APP:
      CreateBookmarkAppFromCurrentWebContents(browser_);
      break;
    case IDC_DEV_TOOLS:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Show());
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::ShowConsole());
      break;
    case IDC_DEV_TOOLS_DEVICES:
      InspectUI::InspectDevices(browser_);
      break;
    case IDC_DEV_TOOLS_INSPECT:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Inspect());
      break;
    case IDC_DEV_TOOLS_TOGGLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Toggle());
      break;
    case IDC_TASK_MANAGER:
      OpenTaskManager(browser_);
      break;
#if defined(OS_CHROMEOS)
    case IDC_TAKE_SCREENSHOT:
      TakeScreenshot();
      break;
#endif
#if defined(GOOGLE_CHROME_BUILD)
    case IDC_FEEDBACK:
      OpenFeedbackDialog(browser_);
      break;
#endif
    case IDC_SHOW_BOOKMARK_BAR:
      ToggleBookmarkBar(browser_);
      break;
    case IDC_PROFILING_ENABLED:
      Profiling::Toggle();
      break;

    case IDC_SHOW_BOOKMARK_MANAGER:
      ShowBookmarkManager(browser_);
      break;
    case IDC_SHOW_APP_MENU:
      content::RecordAction(base::UserMetricsAction("Accel_Show_App_Menu"));
      ShowAppMenu(browser_);
      break;
    case IDC_SHOW_AVATAR_MENU:
      ShowAvatarMenu(browser_);
      break;
    case IDC_SHOW_FAST_USER_SWITCHER:
      ShowFastUserSwitcher(browser_);
      break;
    case IDC_SHOW_HISTORY:
      ShowHistory(browser_);
      break;
    case IDC_SHOW_DOWNLOADS:
      ShowDownloads(browser_);
      break;
    case IDC_MANAGE_EXTENSIONS:
      ShowExtensions(browser_, std::string());
      break;
    case IDC_OPTIONS:
      ShowSettings(browser_);
      break;
    case IDC_EDIT_SEARCH_ENGINES:
      ShowSearchEngineSettings(browser_);
      break;
    case IDC_VIEW_PASSWORDS:
      ShowPasswordManager(browser_);
      break;
    case IDC_CLEAR_BROWSING_DATA:
      ShowClearBrowsingDataDialog(browser_);
      break;
    case IDC_IMPORT_SETTINGS:
      ShowImportDialog(browser_);
      break;
    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      ToggleRequestTabletSite(browser_);
      break;
    case IDC_ABOUT:
      ShowAboutChrome(browser_);
      break;
    case IDC_UPGRADE_DIALOG:
      OpenUpdateChromeDialog(browser_);
      break;
    case IDC_VIEW_INCOMPATIBILITIES:
      ShowConflicts(browser_);
      break;
    case IDC_HELP_PAGE_VIA_KEYBOARD:
      ShowHelp(browser_, HELP_SOURCE_KEYBOARD);
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      ShowHelp(browser_, HELP_SOURCE_MENU);
      break;
    case IDC_SHOW_SIGNIN:
      ShowBrowserSigninOrSettings(
          browser_, signin_metrics::AccessPoint::ACCESS_POINT_MENU);
      break;
    case IDC_DISTILL_PAGE:
      DistillCurrentPage(browser_);
      break;
#if defined(OS_CHROMEOS)
    case IDC_TOUCH_HUD_PROJECTION_TOGGLE:
      ash::accelerators::ToggleTouchHudProjection();
      break;
#endif
    case IDC_ROUTE_MEDIA:
      RouteMedia(browser_);
      break;

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, SigninPrefObserver implementation:

void BrowserCommandController::OnSigninAllowedPrefChange() {
  // For unit tests, we don't have a window.
  if (!window())
    return;
  UpdateShowSyncState(IsShowingMainUI());
}

// BrowserCommandController, TabStripModelObserver implementation:

void BrowserCommandController::TabInsertedAt(WebContents* contents,
                                             int index,
                                             bool foreground) {
  AddInterstitialObservers(contents);
}

void BrowserCommandController::TabDetachedAt(WebContents* contents, int index) {
  RemoveInterstitialObservers(contents);
}

void BrowserCommandController::TabReplacedAt(TabStripModel* tab_strip_model,
                                             WebContents* old_contents,
                                             WebContents* new_contents,
                                             int index) {
  RemoveInterstitialObservers(old_contents);
  AddInterstitialObservers(new_contents);
}

void BrowserCommandController::TabBlockedStateChanged(
    content::WebContents* contents,
    int index) {
  PrintingStateChanged();
  FullscreenStateChanged();
  UpdateCommandsForFind();
  UpdateCommandsForMediaRouter();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, TabRestoreServiceObserver implementation:

void BrowserCommandController::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

void BrowserCommandController::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  service->RemoveObserver(this);
}

void BrowserCommandController::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, private:

class BrowserCommandController::InterstitialObserver
    : public content::WebContentsObserver {
 public:
  InterstitialObserver(BrowserCommandController* controller,
                       content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        controller_(controller) {
  }

  void DidAttachInterstitialPage() override {
    controller_->UpdateCommandsForTabState();
  }

  void DidDetachInterstitialPage() override {
    controller_->UpdateCommandsForTabState();
  }

 private:
  BrowserCommandController* controller_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

bool BrowserCommandController::IsShowingMainUI() {
  bool should_hide_ui = window() && window()->ShouldHideUIForFullscreen();
  return browser_->is_type_tabbed() && !should_hide_ui;
}

void BrowserCommandController::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE, true);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, true);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_NEW_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB, true);
  UpdateTabRestoreCommandState();
#if defined(OS_WIN) && defined(USE_ASH)
  if (browser_->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
#else
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
#endif
  command_updater_.UpdateCommandEnabled(IDC_DEBUG_FRAME_TOGGLE, true);
#if defined(USE_ASH)
  command_updater_.UpdateCommandEnabled(IDC_MINIMIZE_WINDOW, true);
#endif
#if defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_2, true);
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_3, true);
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_USE_SYSTEM_TITLE_BAR, true);
#endif

  // Page-related commands
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION, true);
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_AUTO_DETECT, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF8, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF16LE, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1252, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GBK, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GB18030, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_BIG5, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_THAI, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOREAN, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_SHIFTJIS, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO2022JP, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_EUCJP, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885915, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MACINTOSH, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88592, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1250, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88595, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1251, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOI8R, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOI8U, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_IBM866, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88597, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1253, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88594, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885913, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1257, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88593, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885910, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885914, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885916, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1254, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88596, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1256, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88598, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88598I, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1255, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1258, true);

  // Zoom
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, false);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Show various bits of UI
  const bool guest_session = profile()->IsGuestSession() ||
                             profile()->IsSystemProfile();
  DCHECK(!profile()->IsSystemProfile())
      << "Ought to never have browser for the system profile.";
  const bool normal_window = browser_->is_type_tabbed();
  UpdateOpenFileState(&command_updater_);
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS, false);
  UpdateCommandsForDevTools();
  command_updater_.UpdateCommandEnabled(IDC_TASK_MANAGER, CanOpenTaskManager());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_HISTORY, !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_KEYBOARD, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARKS_MENU, !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_RECENT_TABS_MENU,
                                        !guest_session &&
                                        !profile()->IsOffTheRecord());
  command_updater_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, normal_window);
#if defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_TAKE_SCREENSHOT, true);
  command_updater_.UpdateCommandEnabled(IDC_TOUCH_HUD_PROJECTION_TOGGLE, true);
#else
  // Chrome OS uses the system tray menu to handle multi-profiles.
  if (normal_window && (guest_session || !profile()->IsOffTheRecord())) {
    command_updater_.UpdateCommandEnabled(IDC_SHOW_AVATAR_MENU, true);
    command_updater_.UpdateCommandEnabled(IDC_SHOW_FAST_USER_SWITCHER, true);
  }
#endif

  UpdateShowSyncState(true);

  // Navigation commands
  command_updater_.UpdateCommandEnabled(
      IDC_HOME,
      normal_window ||
          (extensions::util::IsNewBookmarkAppsEnabled() && browser_->is_app()));

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
                                        normal_window);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_NEXT, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_PREVIOUS, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_0, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_1, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_2, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_3, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_4, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_5, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_6, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_7, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_LAST_TAB, normal_window);

  // These are always enabled; the menu determines their menu item visibility.
  command_updater_.UpdateCommandEnabled(IDC_UPGRADE_DIALOG, true);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_INCOMPATIBILITIES, true);

  // Distill current page.
  command_updater_.UpdateCommandEnabled(
      IDC_DISTILL_PAGE, base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kEnableDomDistiller));

  // Initialize other commands whose state changes based on various conditions.
  UpdateCommandsForFullscreenMode();
  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForIncognitoAvailability();
}

// static
void BrowserCommandController::UpdateSharedCommandsForIncognitoAvailability(
    CommandUpdater* command_updater,
    Profile* profile) {
  const bool guest_session = profile->IsGuestSession();
  // TODO(mlerman): Make GetAvailability account for profile->IsGuestSession().
  IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  command_updater->UpdateCommandEnabled(
      IDC_NEW_WINDOW,
      incognito_availability != IncognitoModePrefs::FORCED);
  command_updater->UpdateCommandEnabled(
      IDC_NEW_INCOGNITO_WINDOW,
      incognito_availability != IncognitoModePrefs::DISABLED && !guest_session);

  const bool forced_incognito =
      incognito_availability == IncognitoModePrefs::FORCED ||
      guest_session;  // Guest always runs in Incognito mode.
  command_updater->UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER,
      browser_defaults::bookmarks_enabled && !forced_incognito);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const bool enable_extensions =
      extension_service && extension_service->extensions_enabled();

  // Bookmark manager and settings page/subpages are forced to open in normal
  // mode. For this reason we disable these commands when incognito is forced.
  command_updater->UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS,
                                        enable_extensions && !forced_incognito);

  command_updater->UpdateCommandEnabled(IDC_IMPORT_SETTINGS, !forced_incognito);
  command_updater->UpdateCommandEnabled(IDC_OPTIONS,
                                        !forced_incognito || guest_session);
  command_updater->UpdateCommandEnabled(IDC_SHOW_SIGNIN, !forced_incognito);
}

void BrowserCommandController::UpdateCommandsForIncognitoAvailability() {
  UpdateSharedCommandsForIncognitoAvailability(&command_updater_, profile());

  if (!IsShowingMainUI()) {
    command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, false);
    command_updater_.UpdateCommandEnabled(IDC_OPTIONS, false);
  }
}

void BrowserCommandController::UpdateCommandsForTabState() {
  WebContents* current_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!current_web_contents)  // May be NULL during tab restore.
    return;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_BACK, CanGoBack(browser_));
  command_updater_.UpdateCommandEnabled(IDC_FORWARD, CanGoForward(browser_));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, CanReload(browser_));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE,
                                        CanReload(browser_));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE,
                                        CanReload(browser_));

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB,
      !browser_->is_app() && CanDuplicateTab(browser_));

  // Page-related commands
  window()->SetStarredState(
      BookmarkTabHelper::FromWebContents(current_web_contents)->is_starred());
  window()->ZoomChangedForActiveTab(false);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_SOURCE,
                                        CanViewSource(browser_));
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION,
                                        CanEmailPageLocation(browser_));
  if (browser_->is_devtools())
    command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, false);

  // Changing the encoding is not possible on Chrome-internal webpages.
  NavigationController& nc = current_web_contents->GetController();
  bool is_chrome_internal = HasInternalURL(nc.GetLastCommittedEntry()) ||
      current_web_contents->ShowingInterstitialPage();
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MENU,
      !is_chrome_internal && current_web_contents->IsSavable());

  // Show various bits of UI
  // TODO(pinkerton): Disable app-mode in the model until we implement it
  // on the Mac. Be sure to remove both ifdefs. http://crbug.com/13148
#if !defined(OS_MACOSX)
  command_updater_.UpdateCommandEnabled(
      IDC_CREATE_SHORTCUTS,
      CanCreateApplicationShortcuts(browser_));
#endif
  command_updater_.UpdateCommandEnabled(IDC_CREATE_HOSTED_APP,
                                        CanCreateBookmarkApp(browser_));

  command_updater_.UpdateCommandEnabled(
      IDC_TOGGLE_REQUEST_TABLET_SITE,
      CanRequestTabletSite(current_web_contents));

  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForFind();
  UpdateCommandsForMediaRouter();
  // Update the zoom commands when an active tab is selected.
  UpdateCommandsForZoomState();
}

void BrowserCommandController::UpdateCommandsForZoomState() {
  WebContents* contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS,
                                        CanZoomIn(contents));
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL,
                                        CanResetZoom(contents));
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS,
                                        CanZoomOut(contents));
}

void BrowserCommandController::UpdateCommandsForContentRestrictionState() {
  int restrictions = GetContentRestrictions(browser_);

  command_updater_.UpdateCommandEnabled(
      IDC_COPY, !(restrictions & CONTENT_RESTRICTION_COPY));
  command_updater_.UpdateCommandEnabled(
      IDC_CUT, !(restrictions & CONTENT_RESTRICTION_CUT));
  command_updater_.UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & CONTENT_RESTRICTION_PASTE));
  UpdateSaveAsState();
  UpdatePrintingState();
}

void BrowserCommandController::UpdateCommandsForDevTools() {
  bool dev_tools_enabled =
      !profile()->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_DEVICES,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_TOGGLE,
                                        dev_tools_enabled);
}

void BrowserCommandController::UpdateCommandsForBookmarkEditing() {
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_PAGE,
                                        CanBookmarkCurrentPage(browser_));
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_ALL_TABS,
                                        CanBookmarkAllTabs(browser_));
#if defined(OS_WIN)
  command_updater_.UpdateCommandEnabled(IDC_PIN_TO_START_SCREEN, true);
#endif
}

void BrowserCommandController::UpdateCommandsForBookmarkBar() {
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_BAR,
      browser_defaults::bookmarks_enabled && !profile()->IsGuestSession() &&
          !profile()->IsSystemProfile() &&
          !profile()->GetPrefs()->IsManagedPreference(
              bookmarks::prefs::kShowBookmarkBar) &&
          IsShowingMainUI());
}

void BrowserCommandController::UpdateCommandsForFileSelectionDialogs() {
  UpdateSaveAsState();
  UpdateOpenFileState(&command_updater_);
}

void BrowserCommandController::UpdateCommandsForFullscreenMode() {
  WindowState window_state = WINDOW_STATE_NOT_FULLSCREEN;
  if (window() && window()->IsFullscreen()) {
    window_state = WINDOW_STATE_FULLSCREEN;
#if defined(OS_WIN)
    if (window()->IsInMetroSnapMode())
      window_state = WINDOW_STATE_METRO_SNAP;
#endif
  }
  bool show_main_ui = IsShowingMainUI();
  bool main_not_fullscreen =
      show_main_ui && window_state == WINDOW_STATE_NOT_FULLSCREEN;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_AS_TAB,
      !browser_->is_type_tabbed() &&
          window_state == WINDOW_STATE_NOT_FULLSCREEN);

  // Focus various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, show_main_ui);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_MENU_BAR, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_NEXT_PANE, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_PREVIOUS_PANE, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_BOOKMARKS, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_INFOBARS, main_not_fullscreen);

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
#if defined(GOOGLE_CHROME_BUILD)
  command_updater_.UpdateCommandEnabled(IDC_FEEDBACK, show_main_ui);
#endif
  UpdateShowSyncState(show_main_ui);

  // Settings page/subpages are forced to open in normal mode. We disable these
  // commands for guest sessions and when incognito is forced.
  const bool options_enabled = show_main_ui &&
      IncognitoModePrefs::GetAvailability(
          profile()->GetPrefs()) != IncognitoModePrefs::FORCED;
  const bool guest_session = profile()->IsGuestSession();
  command_updater_.UpdateCommandEnabled(IDC_OPTIONS, options_enabled);
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS,
                                        options_enabled && !guest_session);

  command_updater_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);

  if (base::debug::IsProfilingSupported())
    command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);

  // Disable explicit fullscreen toggling when in metro snap mode.
  bool fullscreen_enabled = window_state != WINDOW_STATE_METRO_SNAP;
#if !defined(OS_MACOSX)
  if (window_state == WINDOW_STATE_NOT_FULLSCREEN &&
      !profile()->GetPrefs()->GetBoolean(prefs::kFullscreenAllowed)) {
    // Disable toggling into fullscreen mode if disallowed by pref.
    fullscreen_enabled = false;
  }
#endif

  command_updater_.UpdateCommandEnabled(IDC_FULLSCREEN, fullscreen_enabled);
  command_updater_.UpdateCommandEnabled(IDC_PRESENTATION_MODE,
                                        fullscreen_enabled);
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_FULLSCREEN_TOOLBAR,
                                        fullscreen_enabled);

  UpdateCommandsForBookmarkBar();
}

void BrowserCommandController::UpdatePrintingState() {
  bool print_enabled = CanPrint(browser_);
  command_updater_.UpdateCommandEnabled(IDC_PRINT, print_enabled);
#if defined(ENABLE_BASIC_PRINTING)
  command_updater_.UpdateCommandEnabled(IDC_BASIC_PRINT,
                                        CanBasicPrint(browser_));
#endif  // ENABLE_BASIC_PRINTING
}

void BrowserCommandController::UpdateSaveAsState() {
  command_updater_.UpdateCommandEnabled(IDC_SAVE_PAGE, CanSavePage(browser_));
}

void BrowserCommandController::UpdateShowSyncState(bool show_main_ui) {
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_SYNC_SETUP, show_main_ui && pref_signin_allowed_.GetValue());
}

// static
void BrowserCommandController::UpdateOpenFileState(
    CommandUpdater* command_updater) {
  bool enabled = true;
  PrefService* local_state = g_browser_process->local_state();
  if (local_state)
    enabled = local_state->GetBoolean(prefs::kAllowFileSelectionDialogs);

  command_updater->UpdateCommandEnabled(IDC_OPEN_FILE, enabled);
}

void BrowserCommandController::UpdateReloadStopState(bool is_loading,
                                                     bool force) {
  window()->UpdateReloadStopState(is_loading, force);
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
}

void BrowserCommandController::UpdateTabRestoreCommandState() {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  // The command is enabled if the service hasn't loaded yet to trigger loading.
  // The command is updated once the load completes.
  command_updater_.UpdateCommandEnabled(
      IDC_RESTORE_TAB,
      tab_restore_service &&
      (!tab_restore_service->IsLoaded() ||
       GetRestoreTabType(browser_) != TabStripModelDelegate::RESTORE_NONE));
}

void BrowserCommandController::UpdateCommandsForFind() {
  TabStripModel* model = browser_->tab_strip_model();
  bool enabled = !model->IsTabBlocked(model->active_index()) &&
                 !browser_->is_devtools();

  command_updater_.UpdateCommandEnabled(IDC_FIND, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_NEXT, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, enabled);
}

void BrowserCommandController::UpdateCommandsForMediaRouter() {
  command_updater_.UpdateCommandEnabled(IDC_ROUTE_MEDIA,
                                        CanRouteMedia(browser_));
}

void BrowserCommandController::AddInterstitialObservers(WebContents* contents) {
  interstitial_observers_.push_back(new InterstitialObserver(this, contents));
}

void BrowserCommandController::RemoveInterstitialObservers(
    WebContents* contents) {
  for (size_t i = 0; i < interstitial_observers_.size(); i++) {
    if (interstitial_observers_[i]->web_contents() != contents)
      continue;

    delete interstitial_observers_[i];
    interstitial_observers_.erase(interstitial_observers_.begin() + i);
    return;
  }
}

BrowserWindow* BrowserCommandController::window() {
  return browser_->window();
}

Profile* BrowserCommandController::profile() {
  return browser_->profile();
}

}  // namespace chrome
