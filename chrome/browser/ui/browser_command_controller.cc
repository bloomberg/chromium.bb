// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/url_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "base/win/windows_version.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_util.h"
#endif

using content::NavigationEntry;
using content::NavigationController;
using content::WebContents;

namespace {

// Returns |true| if entry has an internal chrome:// URL, |false| otherwise.
bool HasInternalURL(const NavigationEntry* entry) {
  if (!entry)
    return false;

  // Check the |virtual_url()| first. This catches regular chrome:// URLs
  // including URLs that were rewritten (such as chrome://bookmarks).
  if (entry->GetVirtualURL().SchemeIs(chrome::kChromeUIScheme))
    return true;

  // If the |virtual_url()| isn't a chrome:// URL, check if it's actually
  // view-source: of a chrome:// URL.
  if (entry->GetVirtualURL().SchemeIs(chrome::kViewSourceScheme))
    return entry->GetURL().SchemeIs(chrome::kChromeUIScheme);

  return false;
}

#if defined(OS_WIN)
// Windows 8 specific helper class to manage DefaultBrowserWorker. It does the
// following asynchronous actions in order:
// 1- Check that chrome is the default browser
// 2- If we are the default, restart chrome in metro and exit
// 3- If not the default browser show the 'select default browser' system dialog
// 4- When dialog dismisses check again who got made the default
// 5- If we are the default then restart chrome in metro and exit
// 6- If we are not the default exit.
//
// Note: this class deletes itself.
class SwichToMetroUIHandler
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  SwichToMetroUIHandler()
      : ALLOW_THIS_IN_INITIALIZER_LIST(default_browser_worker_(
            new ShellIntegration::DefaultBrowserWorker(this))),
        first_check_(true) {
    default_browser_worker_->StartCheckIsDefault();
  }

  virtual ~SwichToMetroUIHandler() {
    default_browser_worker_->ObserverDestroyed();
  }

 private:
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE {
    switch (state) {
      case ShellIntegration::STATE_PROCESSING:
        return;
      case ShellIntegration::STATE_UNKNOWN :
        break;
      case ShellIntegration::STATE_IS_DEFAULT:
        chrome::AttemptRestartWithModeSwitch();
        break;
      case ShellIntegration::STATE_NOT_DEFAULT:
        if (first_check_) {
          default_browser_worker_->StartSetAsDefault();
          return;
        }
        break;
      default:
        NOTREACHED();
    }
    delete this;
  }

  virtual void OnSetAsDefaultConcluded(bool success)  OVERRIDE {
    if (!success) {
      delete this;
      return;
    }
    first_check_ = false;
    default_browser_worker_->StartCheckIsDefault();
  }

  virtual bool IsInteractiveSetDefaultPermitted() OVERRIDE {
    return true;
  }

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;
  bool first_check_;

  DISALLOW_COPY_AND_ASSIGN(SwichToMetroUIHandler);
};
#endif  // defined(OS_WIN)

}  // namespace

namespace chrome {

///////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, public:

BrowserCommandController::BrowserCommandController(
    Browser* browser,
    ProfileManager* profile_manager)
    : browser_(browser),
      profile_manager_(profile_manager),
      ALLOW_THIS_IN_INITIALIZER_LIST(command_updater_(this)),
      block_command_execution_(false),
      last_blocked_command_id_(-1),
      last_blocked_command_disposition_(CURRENT_TAB) {
  if (profile_manager_)
    profile_manager_->GetProfileInfoCache().AddObserver(this);
  browser_->tab_strip_model()->AddObserver(this);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(
        prefs::kAllowFileSelectionDialogs,
        base::Bind(
            &BrowserCommandController::UpdateCommandsForFileSelectionDialogs,
            base::Unretained(this)));
    local_pref_registrar_.Add(
        prefs::kInManagedMode,
        base::Bind(
            &BrowserCommandController::UpdateCommandsForMultipleProfiles,
            base::Unretained(this)));
  }

  profile_pref_registrar_.Init(profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDevToolsDisabled,
      base::Bind(&BrowserCommandController::UpdateCommandsForDevTools,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kEditBookmarksEnabled,
      base::Bind(&BrowserCommandController::UpdateCommandsForBookmarkEditing,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kShowBookmarkBar,
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

  InitCommandState();

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service) {
    tab_restore_service->AddObserver(this);
    TabRestoreServiceChanged(tab_restore_service);
  }
}

BrowserCommandController::~BrowserCommandController() {
  // TabRestoreService may have been shutdown by the time we get here. Don't
  // trigger creating it.
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfileIfExisting(profile());
  if (tab_restore_service)
    tab_restore_service->RemoveObserver(this);
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  browser_->tab_strip_model()->RemoveObserver(this);
  if (profile_manager_)
    profile_manager_->GetProfileInfoCache().RemoveObserver(this);
}

bool BrowserCommandController::IsReservedCommandOrKey(
    int command_id,
    const content::NativeWebKeyboardEvent& event) {
  // In Apps mode, no keys are reserved.
  if (browser_->is_app())
    return false;

#if defined(OS_CHROMEOS)
  // On Chrome OS, the top row of keys are mapped to F1-F10.  We don't want web
  // pages to be able to change the behavior of these keys.  Ash handles F4 and
  // up; this leaves us needing to reserve F1-F3 here.
  ui::KeyboardCode key_code =
    static_cast<ui::KeyboardCode>(event.windowsKeyCode);
  if ((key_code == ui::VKEY_F1 && command_id == IDC_BACK) ||
      (key_code == ui::VKEY_F2 && command_id == IDC_FORWARD) ||
      (key_code == ui::VKEY_F3 && command_id == IDC_RELOAD)) {
    return true;
  }
#endif

  if (window()->IsFullscreen() && command_id == IDC_FULLSCREEN)
    return true;
  return command_id == IDC_CLOSE_TAB ||
         command_id == IDC_CLOSE_WINDOW ||
         command_id == IDC_NEW_INCOGNITO_WINDOW ||
         command_id == IDC_NEW_TAB ||
         command_id == IDC_NEW_WINDOW ||
         command_id == IDC_RESTORE_TAB ||
         command_id == IDC_SELECT_NEXT_TAB ||
         command_id == IDC_SELECT_PREVIOUS_TAB ||
         command_id == IDC_TABPOSE ||
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

void BrowserCommandController::ContentRestrictionsChanged() {
  UpdateCommandsForContentRestrictionState();
}

void BrowserCommandController::FullscreenStateChanged() {
  FullScreenMode fullscreen_mode = FULLSCREEN_DISABLED;
  if (window()->IsFullscreen()) {
#if defined(OS_WIN)
    fullscreen_mode = window()->IsInMetroSnapMode() ? FULLSCREEN_METRO_SNAP :
                                                      FULLSCREEN_NORMAL;
#else
    fullscreen_mode = FULLSCREEN_NORMAL;
#endif
  }
  UpdateCommandsForFullscreenMode(fullscreen_mode);
}

void BrowserCommandController::PrintingStateChanged() {
  UpdatePrintingState();
}

void BrowserCommandController::LoadingStateChanged(bool is_loading,
                                                   bool force) {
  UpdateReloadStopState(is_loading, force);
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
      CloseWindow(browser_);
      break;
    case IDC_NEW_TAB:
      NewTab(browser_);
      break;
    case IDC_CLOSE_TAB:
      CloseTab(browser_);
      break;
    case IDC_SELECT_NEXT_TAB:
      SelectNextTab(browser_);
      break;
    case IDC_SELECT_PREVIOUS_TAB:
      SelectPreviousTab(browser_);
      break;
    case IDC_TABPOSE:
      OpenTabpose(browser_);
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
      chrome::ToggleFullscreenWithChromeOrFallback(browser_);
#else
      chrome::ToggleFullscreenMode(browser_);
#endif
      break;

#if defined(USE_ASH)
    case IDC_TOGGLE_ASH_DESKTOP:
      chrome::ToggleAshDesktop();
      break;
#endif

#if defined(OS_WIN)
    // Windows 8 specific commands.
    case IDC_METRO_SNAP_ENABLE:
      browser_->SetMetroSnapMode(true);
      break;
    case IDC_METRO_SNAP_DISABLE:
      browser_->SetMetroSnapMode(false);
      break;
    case IDC_WIN8_DESKTOP_RESTART:
      chrome::AttemptRestartWithModeSwitch();
      content::RecordAction(content::UserMetricsAction("Win8DesktopRestart"));
      break;
    case IDC_WIN8_METRO_RESTART:
      new SwichToMetroUIHandler;
      content::RecordAction(content::UserMetricsAction("Win8MetroRestart"));
      break;
#endif

#if defined(OS_MACOSX)
    case IDC_PRESENTATION_MODE:
      chrome::ToggleFullscreenMode(browser_);
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
      BookmarkCurrentPage(browser_);
      break;
    case IDC_BOOKMARK_PAGE_FROM_STAR:
      BookmarkCurrentPageFromStar(browser_);
      break;
    case IDC_PIN_TO_START_SCREEN:
      TogglePagePinnedToStartScreen(browser_);
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
    case IDC_ADVANCED_PRINT:
      AdvancedPrint(browser_);
      break;
    case IDC_PRINT_TO_DESTINATION:
      PrintToDestination(browser_);
      break;
    case IDC_CHROME_TO_MOBILE_PAGE:
      ShowChromeToMobileBubble(browser_);
      break;
    case IDC_ENCODING_AUTO_DETECT:
      browser_->ToggleEncodingAutoDetect();
      break;
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_ISO88591:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GBK:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5HKSCS:
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
      Cut(browser_);
      break;
    case IDC_COPY:
      Copy(browser_);
      break;
    case IDC_PASTE:
      Paste(browser_);
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
      FocusToolbar(browser_);
      break;
    case IDC_FOCUS_LOCATION:
      FocusLocationBar(browser_);
      break;
    case IDC_FOCUS_SEARCH:
      FocusSearch(browser_);
      break;
    case IDC_FOCUS_MENU_BAR:
      FocusAppMenu(browser_);
      break;
    case IDC_FOCUS_BOOKMARKS:
      FocusBookmarksToolbar(browser_);
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
    case IDC_DEV_TOOLS:
      ToggleDevToolsWindow(browser_, DEVTOOLS_TOGGLE_ACTION_SHOW);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      ToggleDevToolsWindow(browser_, DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
      break;
    case IDC_DEV_TOOLS_INSPECT:
      ToggleDevToolsWindow(browser_, DEVTOOLS_TOGGLE_ACTION_INSPECT);
      break;
    case IDC_DEV_TOOLS_TOGGLE:
      ToggleDevToolsWindow(browser_, DEVTOOLS_TOGGLE_ACTION_TOGGLE);
      break;
    case IDC_TASK_MANAGER:
      OpenTaskManager(browser_, false);
      break;
    case IDC_VIEW_BACKGROUND_PAGES:
      OpenTaskManager(browser_, true);
      break;
    case IDC_FEEDBACK:
      OpenFeedbackDialog(browser_);
      break;

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
      ShowAppMenu(browser_);
      break;
    case IDC_SHOW_AVATAR_MENU:
      ShowAvatarMenu(browser_);
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
      ShowBrowserSignin(browser_, SyncPromoUI::SOURCE_MENU);
      break;
    case IDC_TOGGLE_SPEECH_INPUT:
      ToggleSpeechInput(browser_);
      break;

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, content::NotificationObserver implementation:

void BrowserCommandController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_INTERSTITIAL_ATTACHED:
      UpdateCommandsForTabState();
      break;

    case content::NOTIFICATION_INTERSTITIAL_DETACHED:
      UpdateCommandsForTabState();
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}


////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, ProfileInfoCacheObserver implementation:

void BrowserCommandController::OnProfileAdded(
    const base::FilePath& profile_path) {
  UpdateCommandsForMultipleProfiles();
}

void BrowserCommandController::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const string16& profile_name) {
  UpdateCommandsForMultipleProfiles();
}

void BrowserCommandController::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
}

void BrowserCommandController::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const string16& old_profile_name) {
}

void BrowserCommandController::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
}

////////////////////////////////////////////////////////////////////////////////
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
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, TabRestoreServiceObserver implementation:

void BrowserCommandController::TabRestoreServiceChanged(
    TabRestoreService* service) {
  command_updater_.UpdateCommandEnabled(
      IDC_RESTORE_TAB,
      GetRestoreTabType(browser_) != TabStripModelDelegate::RESTORE_NONE);
}

void BrowserCommandController::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  service->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, private:

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
  command_updater_.UpdateCommandEnabled(IDC_RESTORE_TAB, false);
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
  command_updater_.UpdateCommandEnabled(IDC_DEBUG_FRAME_TOGGLE, true);
#if defined(OS_WIN) && defined(USE_ASH) && !defined(NDEBUG)
  if (base::win::GetVersion() < base::win::VERSION_WIN8 &&
      chrome::HOST_DESKTOP_TYPE_NATIVE != chrome::HOST_DESKTOP_TYPE_ASH)
    command_updater_.UpdateCommandEnabled(IDC_TOGGLE_ASH_DESKTOP, true);
#endif

  // Page-related commands
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_AUTO_DETECT, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF8, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF16LE, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88591, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1252, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GBK, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GB18030, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_BIG5HKSCS, true);
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
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Show various bits of UI
  UpdateOpenFileState(&command_updater_);
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS, false);
  UpdateCommandsForDevTools();
  command_updater_.UpdateCommandEnabled(IDC_TASK_MANAGER, CanOpenTaskManager());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_HISTORY,
                                        !profile()->IsGuestSession());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_KEYBOARD, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARKS_MENU,
                                        !profile()->IsGuestSession());
  command_updater_.UpdateCommandEnabled(IDC_RECENT_TABS_MENU,
                                        !profile()->IsGuestSession() &&
                                        !profile()->IsOffTheRecord());

  command_updater_.UpdateCommandEnabled(IDC_SHOW_SIGNIN, true);

  // Initialize other commands based on the window type.
  bool normal_window = browser_->is_type_tabbed();

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_HOME, normal_window);

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
#if defined(OS_WIN)
  const bool metro_mode = base::win::IsMetroProcess();
  command_updater_.UpdateCommandEnabled(IDC_METRO_SNAP_ENABLE, metro_mode);
  command_updater_.UpdateCommandEnabled(IDC_METRO_SNAP_DISABLE, metro_mode);
  int restart_mode = metro_mode ?
      IDC_WIN8_DESKTOP_RESTART : IDC_WIN8_METRO_RESTART;
  command_updater_.UpdateCommandEnabled(restart_mode, normal_window);
#endif
  command_updater_.UpdateCommandEnabled(IDC_TABPOSE, normal_window);

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, normal_window);

  // The upgrade entry and the view incompatibility entry should always be
  // enabled. Whether they are visible is a separate matter determined on menu
  // show.
  command_updater_.UpdateCommandEnabled(IDC_UPGRADE_DIALOG, true);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_INCOMPATIBILITIES, true);

  // View Background Pages entry is always enabled, but is hidden if there are
  // no background pages.
  command_updater_.UpdateCommandEnabled(IDC_VIEW_BACKGROUND_PAGES, true);

  // Toggle speech input
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_SPEECH_INPUT, true);

  // Initialize other commands whose state changes based on fullscreen mode.
  UpdateCommandsForFullscreenMode(FULLSCREEN_DISABLED);

  UpdateCommandsForContentRestrictionState();

  UpdateCommandsForBookmarkEditing();

  UpdateCommandsForIncognitoAvailability();
}

// static
void BrowserCommandController::UpdateSharedCommandsForIncognitoAvailability(
    CommandUpdater* command_updater,
    Profile* profile) {
  IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  command_updater->UpdateCommandEnabled(
      IDC_NEW_WINDOW,
      incognito_availability != IncognitoModePrefs::FORCED);
  command_updater->UpdateCommandEnabled(
      IDC_NEW_INCOGNITO_WINDOW,
      incognito_availability != IncognitoModePrefs::DISABLED);

  // Bookmark manager and settings page/subpages are forced to open in normal
  // mode. For this reason we disable these commands when incognito is forced.
  const bool command_enabled =
      incognito_availability != IncognitoModePrefs::FORCED;
  command_updater->UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER,
      browser_defaults::bookmarks_enabled && command_enabled);
  ExtensionService* extension_service = profile->GetExtensionService();
  bool enable_extensions =
      extension_service && extension_service->extensions_enabled();
  command_updater->UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS,
                                        enable_extensions && command_enabled);

  command_updater->UpdateCommandEnabled(IDC_IMPORT_SETTINGS, command_enabled);
  command_updater->UpdateCommandEnabled(IDC_OPTIONS, command_enabled);
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
  bool is_chrome_internal = HasInternalURL(nc.GetActiveEntry()) ||
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

  command_updater_.UpdateCommandEnabled(
      IDC_TOGGLE_REQUEST_TABLET_SITE,
      CanRequestTabletSite(current_web_contents));

  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForFind();
}

void BrowserCommandController::UpdateCommandsForContentRestrictionState() {
  int restrictions = GetContentRestrictions(browser_);

  command_updater_.UpdateCommandEnabled(
      IDC_COPY, !(restrictions & content::CONTENT_RESTRICTION_COPY));
  command_updater_.UpdateCommandEnabled(
      IDC_CUT, !(restrictions & content::CONTENT_RESTRICTION_CUT));
  command_updater_.UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & content::CONTENT_RESTRICTION_PASTE));
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
  command_updater_.UpdateCommandEnabled(IDC_PIN_TO_START_SCREEN,
                                        true);
}

void BrowserCommandController::UpdateCommandsForBookmarkBar() {
  command_updater_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_BAR,
      browser_defaults::bookmarks_enabled &&
      !profile()->GetPrefs()->IsManagedPreference(prefs::kShowBookmarkBar) &&
      IsShowingMainUI());
}

void BrowserCommandController::UpdateCommandsForFileSelectionDialogs() {
  UpdateSaveAsState();
  UpdateOpenFileState(&command_updater_);
}

void BrowserCommandController::UpdateCommandsForFullscreenMode(
    FullScreenMode fullscreen_mode) {
  bool show_main_ui = IsShowingMainUI();
  bool main_not_fullscreen = show_main_ui &&
                             (fullscreen_mode == FULLSCREEN_DISABLED);

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_AS_TAB,
      !browser_->is_type_tabbed() && fullscreen_mode == FULLSCREEN_DISABLED);

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

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FEEDBACK, show_main_ui);

  // Settings page/subpages are forced to open in normal mode. We disable these
  // commands when incognito is forced.
  const bool options_enabled = show_main_ui &&
      IncognitoModePrefs::GetAvailability(
          profile()->GetPrefs()) != IncognitoModePrefs::FORCED;
  command_updater_.UpdateCommandEnabled(IDC_OPTIONS, options_enabled);
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, options_enabled);

  command_updater_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);
#if defined (ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);
#endif

  // Disable explicit fullscreen toggling for app-panels and when in metro snap
  // mode.
  bool fullscreen_enabled =
      !(browser_->is_type_panel() && browser_->is_app()) &&
      fullscreen_mode != FULLSCREEN_METRO_SNAP;
#if defined(OS_MACOSX)
  // The Mac implementation doesn't support switching to fullscreen while
  // a tab modal dialog is displayed.
  int tab_index = chrome::IndexOfFirstBlockedTab(browser_->tab_strip_model());
  bool has_blocked_tab = tab_index != browser_->tab_strip_model()->count();
  fullscreen_enabled &= !has_blocked_tab;
#endif

  command_updater_.UpdateCommandEnabled(IDC_FULLSCREEN, fullscreen_enabled);
  command_updater_.UpdateCommandEnabled(IDC_PRESENTATION_MODE,
                                        fullscreen_enabled);

  UpdateCommandsForBookmarkBar();
  UpdateCommandsForMultipleProfiles();
}

void BrowserCommandController::UpdateCommandsForMultipleProfiles() {
  bool enable = IsShowingMainUI() &&
      !profile()->IsOffTheRecord() &&
      profile_manager_ &&
      AvatarMenuModel::ShouldShowAvatarMenu();
  command_updater_.UpdateCommandEnabled(IDC_SHOW_AVATAR_MENU,
                                        enable);
}

void BrowserCommandController::UpdatePrintingState() {
  bool print_enabled = CanPrint(browser_);
  command_updater_.UpdateCommandEnabled(IDC_PRINT, print_enabled);
  command_updater_.UpdateCommandEnabled(IDC_ADVANCED_PRINT,
                                        CanAdvancedPrint(browser_));
  command_updater_.UpdateCommandEnabled(IDC_PRINT_TO_DESTINATION,
                                        print_enabled);
#if defined(OS_WIN)
  HMODULE metro_module = base::win::GetMetroModule();
  if (metro_module != NULL) {
    typedef void (*MetroEnablePrinting)(BOOL);
    MetroEnablePrinting metro_enable_printing =
        reinterpret_cast<MetroEnablePrinting>(
            ::GetProcAddress(metro_module, "MetroEnablePrinting"));
    if (metro_enable_printing)
      metro_enable_printing(print_enabled);
  }
#endif
}

void BrowserCommandController::UpdateSaveAsState() {
  command_updater_.UpdateCommandEnabled(IDC_SAVE_PAGE, CanSavePage(browser_));
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

void BrowserCommandController::UpdateCommandsForFind() {
  TabStripModel* model = browser_->tab_strip_model();
  bool enabled = !model->IsTabBlocked(model->active_index()) &&
                 !browser_->is_devtools();

  command_updater_.UpdateCommandEnabled(IDC_FIND, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_NEXT, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, enabled);
}

void BrowserCommandController::AddInterstitialObservers(WebContents* contents) {
  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                 content::Source<WebContents>(contents));
  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                 content::Source<WebContents>(contents));
}

void BrowserCommandController::RemoveInterstitialObservers(
    WebContents* contents) {
  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                    content::Source<WebContents>(contents));
  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                    content::Source<WebContents>(contents));
}

BrowserWindow* BrowserCommandController::window() {
  return browser_->window();
}

Profile* BrowserCommandController::profile() {
  return browser_->profile();
}

}  // namespace chrome
