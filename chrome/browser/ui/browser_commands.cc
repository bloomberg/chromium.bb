// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_prompt_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/user_agent/user_agent_util.h"

#if defined(OS_MACOSX)
#include "ui/base/cocoa/find_pasteboard.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/metro_pin_tab_helper_win.h"
#include "win8/util/win8_util.h"
#endif

namespace {
const char kOsOverrideForTabletSite[] = "Linux; Android 4.0.3";
}

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::SSLStatus;
using content::UserMetricsAction;
using content::WebContents;

namespace chrome {
namespace {

void BookmarkCurrentPageInternal(Browser* browser, bool from_star) {
  content::RecordAction(UserMetricsAction("Star"));

  BookmarkModel* model =
      BookmarkModelFactory::GetForProfile(browser->profile());
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  GURL url;
  string16 title;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  GetURLAndTitleToBookmark(web_contents, &url, &title);
  bool was_bookmarked = model->IsBookmarked(url);
  if (!was_bookmarked && web_contents->GetBrowserContext()->IsOffTheRecord()) {
    // If we're incognito the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    FaviconTabHelper::FromWebContents(web_contents)->SaveFavicon();
  }
  bookmark_utils::AddIfNotBookmarked(model, url, title);
  if (from_star && !was_bookmarked)
    BookmarkPromptController::AddedBookmark(browser, url);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (browser->window()->IsActive() && model->IsBookmarked(url)) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations where the bubble is deleted as soon as it is shown.
    browser->window()->ShowBookmarkBubble(url, was_bookmarked);
  }
}

WebContents* GetOrCloneTabForDisposition(Browser* browser,
                                         WindowOpenDisposition disposition) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  switch (disposition) {
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB: {
      current_tab = current_tab->Clone();
      browser->tab_strip_model()->AddWebContents(
          current_tab, -1, content::PAGE_TRANSITION_LINK,
          disposition == NEW_FOREGROUND_TAB ? TabStripModel::ADD_ACTIVE :
                                              TabStripModel::ADD_NONE);
      break;
    }
    case NEW_WINDOW: {
      current_tab = current_tab->Clone();
      Browser* b = new Browser(Browser::CreateParams(
          browser->profile(), browser->host_desktop_type()));
      b->tab_strip_model()->AddWebContents(
          current_tab, -1, content::PAGE_TRANSITION_LINK,
          TabStripModel::ADD_ACTIVE);
      b->window()->Show();
      break;
    }
    default:
      break;
  }
  return current_tab;
}

void ReloadInternal(Browser* browser,
                    WindowOpenDisposition disposition,
                    bool ignore_cache) {
  // As this is caused by a user action, give the focus to the page.
  //
  // Also notify RenderViewHostDelegate of the user gesture; this is
  // normally done in Browser::Navigate, but a reload bypasses Navigate.
  WebContents* web_contents = GetOrCloneTabForDisposition(browser, disposition);
  web_contents->UserGestureDone();
  if (!web_contents->FocusLocationBarByDefault())
    web_contents->Focus();
  if (ignore_cache)
    web_contents->GetController().ReloadIgnoringCache(true);
  else
    web_contents->GetController().Reload(true);
}

bool IsShowingWebContentsModalDialog(const Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  return web_contents_modal_dialog_manager->IsShowingDialog();
}

bool PrintPreviewShowing(const Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  printing::PrintPreviewDialogController* controller =
      printing::PrintPreviewDialogController::GetInstance();
  return controller && (controller->GetPrintPreviewForTab(contents) ||
                        controller->is_creating_print_preview_tab());
}

bool IsNTPModeForInstantExtendedAPI(const Browser* browser) {
  return browser->search_model() &&
      search::IsInstantExtendedAPIEnabled(browser->profile()) &&
          browser->search_model()->mode().is_ntp();
}

}  // namespace

bool IsCommandEnabled(Browser* browser, int command) {
  return browser->command_controller()->command_updater()->IsCommandEnabled(
      command);
}

bool SupportsCommand(Browser* browser, int command) {
  return browser->command_controller()->command_updater()->SupportsCommand(
      command);
}

bool ExecuteCommand(Browser* browser, int command) {
  return browser->command_controller()->command_updater()->ExecuteCommand(
      command);
}

bool ExecuteCommandWithDisposition(Browser* browser,
                                   int command,
                                   WindowOpenDisposition disposition) {
  return browser->command_controller()->command_updater()->
      ExecuteCommandWithDisposition(command, disposition);
}

void UpdateCommandEnabled(Browser* browser, int command, bool enabled) {
  browser->command_controller()->command_updater()->UpdateCommandEnabled(
      command, enabled);
}

void AddCommandObserver(Browser* browser,
                        int command,
                        CommandObserver* observer) {
  browser->command_controller()->command_updater()->AddCommandObserver(
      command, observer);
}

void RemoveCommandObserver(Browser* browser,
                           int command,
                           CommandObserver* observer) {
  browser->command_controller()->command_updater()->RemoveCommandObserver(
      command, observer);
}

int GetContentRestrictions(const Browser* browser) {
  int content_restrictions = 0;
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (current_tab) {
    content_restrictions = current_tab->GetContentRestrictions();
    NavigationEntry* active_entry =
        current_tab->GetController().GetActiveEntry();
    // See comment in UpdateCommandsForTabState about why we call url().
    if (!download_util::IsSavableURL(
            active_entry ? active_entry->GetURL() : GURL()) ||
        current_tab->ShowingInterstitialPage())
      content_restrictions |= content::CONTENT_RESTRICTION_SAVE;
    if (current_tab->ShowingInterstitialPage())
      content_restrictions |= content::CONTENT_RESTRICTION_PRINT;
  }
  return content_restrictions;
}

void NewEmptyWindow(Profile* profile, HostDesktopType desktop_type) {
  bool incognito = profile->IsOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  if (incognito) {
    if (IncognitoModePrefs::GetAvailability(prefs) ==
          IncognitoModePrefs::DISABLED) {
      incognito = false;
    }
  } else {
    if (browser_defaults::kAlwaysOpenIncognitoWindow &&
        IncognitoModePrefs::ShouldLaunchIncognito(
            *CommandLine::ForCurrentProcess(), prefs)) {
      incognito = true;
    }
  }

  if (incognito) {
    content::RecordAction(UserMetricsAction("NewIncognitoWindow"));
    OpenEmptyWindow(profile->GetOffTheRecordProfile(), desktop_type);
  } else {
    content::RecordAction(UserMetricsAction("NewWindow"));
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile->GetOriginalProfile());
    if (!session_service ||
        !session_service->RestoreIfNecessary(std::vector<GURL>())) {
      OpenEmptyWindow(profile->GetOriginalProfile(), desktop_type);
    }
  }
}

void NewEmptyWindow(Profile* profile) {
  NewEmptyWindow(profile, HOST_DESKTOP_TYPE_NATIVE);
}

Browser* OpenEmptyWindow(Profile* profile, HostDesktopType desktop_type) {
  // TODO(scottmg): http://crbug.com/128578
  // This is necessary because WebContentsViewAura doesn't have enough context
  // to get the right StackingClient (and therefore parent window) otherwise.
  ScopedForceDesktopType force_desktop_type(desktop_type);
  Browser* browser = new Browser(
      Browser::CreateParams(Browser::TYPE_TABBED, profile, desktop_type));
  AddBlankTabAt(browser, -1, true);
  browser->window()->Show();
  return browser;
}

Browser* OpenEmptyWindow(Profile* profile) {
  return OpenEmptyWindow(profile, HOST_DESKTOP_TYPE_NATIVE);
}

void OpenWindowWithRestoredTabs(Profile* profile) {
  TabRestoreService* service = TabRestoreServiceFactory::GetForProfile(profile);
  if (service)
    service->RestoreMostRecentEntry(NULL);
}

void OpenURLOffTheRecord(Profile* profile,
                         const GURL& url,
                         chrome::HostDesktopType desktop_type) {
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile->GetOffTheRecordProfile(), desktop_type);
  AddSelectedTabWithURL(browser, url, content::PAGE_TRANSITION_LINK);
  browser->window()->Show();
}

bool CanGoBack(const Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents()->
      GetController().CanGoBack();
}

void GoBack(Browser* browser, WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Back"));

  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (CanGoBack(browser)) {
    WebContents* new_tab = GetOrCloneTabForDisposition(browser, disposition);
    // If we are on an interstitial page and clone the tab, it won't be copied
    // to the new tab, so we don't need to go back.
    if (current_tab->ShowingInterstitialPage() && new_tab != current_tab)
      return;
    new_tab->GetController().GoBack();
  }
}

bool CanGoForward(const Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents()->
      GetController().CanGoForward();
}

void GoForward(Browser* browser, WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(browser)) {
    GetOrCloneTabForDisposition(browser, disposition)->
        GetController().GoForward();
  }
}

bool NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disp) {
  NavigationController& controller =
      GetOrCloneTabForDisposition(browser, disp)->GetController();
  if (index < 0 || index >= controller.GetEntryCount())
    return false;
  controller.GoToIndex(index);
  return true;
}

void Reload(Browser* browser, WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(browser, disposition, false);
}

void ReloadIgnoringCache(Browser* browser, WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("ReloadIgnoringCache"));
  ReloadInternal(browser, disposition, true);
}

bool CanReload(const Browser* browser) {
  return !browser->is_devtools() && !IsNTPModeForInstantExtendedAPI(browser);
}

void Home(Browser* browser, WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Home"));

  std::string extra_headers;
#if defined(ENABLE_RLZ)
  // If the home page is a Google home page, add the RLZ header to the request.
  PrefService* pref_service = browser->profile()->GetPrefs();
  if (pref_service) {
    std::string home_page = pref_service->GetString(prefs::kHomePage);
    if (google_util::IsGoogleHomePageUrl(home_page)) {
      extra_headers = RLZTracker::GetAccessPointHttpHeader(
          RLZTracker::CHROME_HOME_PAGE);
    }
  }
#endif

  OpenURLParams params(
      browser->profile()->GetHomePage(), Referrer(), disposition,
      content::PageTransitionFromInt(
          content::PAGE_TRANSITION_AUTO_BOOKMARK |
          content::PAGE_TRANSITION_HOME_PAGE),
      false);
  params.extra_headers = extra_headers;
  browser->OpenURL(params);
}

void OpenCurrentURL(Browser* browser) {
  content::RecordAction(UserMetricsAction("LoadURL"));
  LocationBar* location_bar = browser->window()->GetLocationBar();
  if (!location_bar)
    return;

  WindowOpenDisposition open_disposition =
      location_bar->GetWindowOpenDisposition();
  if (browser->instant_controller() &&
      browser->instant_controller()->OpenInstant(open_disposition))
    return;

  GURL url(location_bar->GetInputString());

  NavigateParams params(browser, url, location_bar->GetPageTransition());
  params.disposition = open_disposition;
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      TabStripModel::ADD_FORCE_INDEX | TabStripModel::ADD_INHERIT_OPENER;
  Navigate(&params);

  DCHECK(browser->profile()->GetExtensionService());
  if (browser->profile()->GetExtensionService()->IsInstalledApp(url)) {
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_LOCATION);
  }
}

void Stop(Browser* browser) {
  content::RecordAction(UserMetricsAction("Stop"));
  browser->tab_strip_model()->GetActiveWebContents()->Stop();
}

#if !defined(OS_WIN)
void NewWindow(Browser* browser) {
  NewEmptyWindow(browser->profile()->GetOriginalProfile());
}

void NewIncognitoWindow(Browser* browser) {
  NewEmptyWindow(browser->profile()->GetOffTheRecordProfile());
}
#endif  // OS_WIN

void CloseWindow(Browser* browser) {
  content::RecordAction(UserMetricsAction("CloseWindow"));
  browser->window()->Close();
}

void NewTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("NewTab"));
  // TODO(asvitkine): This is invoked programmatically from several places.
  // Audit the code and change it so that the histogram only gets collected for
  // user-initiated commands.
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", TabStripModel::NEW_TAB_COMMAND,
                            TabStripModel::NEW_TAB_ENUM_COUNT);

  if (browser->is_type_tabbed()) {
    AddBlankTabAt(browser, -1, true);
    browser->tab_strip_model()->GetActiveWebContents()->GetView()->
        RestoreFocus();
  } else {
    Browser* b =
        chrome::FindOrCreateTabbedBrowser(browser->profile(),
                                          browser->host_desktop_type());
    AddBlankTabAt(b, -1, true);
    b->window()->Show();
    // The call to AddBlankTabAt above did not set the focus to the tab as its
    // window was not active, so we have to do it explicitly.
    // See http://crbug.com/6380.
    b->tab_strip_model()->GetActiveWebContents()->GetView()->RestoreFocus();
  }
}

void CloseTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("CloseTab_Accelerator"));
  browser->tab_strip_model()->CloseSelectedTabs();
}

void RestoreTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("RestoreTab"));
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  if (service)
    service->RestoreMostRecentEntry(browser->tab_restore_service_delegate());
}

bool CanRestoreTab(const Browser* browser) {
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  return service && !service->entries().empty();
}

void SelectNextTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("SelectNextTab"));
  browser->tab_strip_model()->SelectNextTab();
}

void SelectPreviousTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("SelectPrevTab"));
  browser->tab_strip_model()->SelectPreviousTab();
}

void OpenTabpose(Browser* browser) {
#if defined(OS_MACOSX)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExposeForTabs)) {
    return;
  }

  content::RecordAction(UserMetricsAction("OpenTabpose"));
  browser->window()->OpenTabpose();
#else
  NOTREACHED();
#endif
}

void MoveTabNext(Browser* browser) {
  content::RecordAction(UserMetricsAction("MoveTabNext"));
  browser->tab_strip_model()->MoveTabNext();
}

void MoveTabPrevious(Browser* browser) {
  content::RecordAction(UserMetricsAction("MoveTabPrevious"));
  browser->tab_strip_model()->MoveTabPrevious();
}

void SelectNumberedTab(Browser* browser, int index) {
  if (index < browser->tab_strip_model()->count()) {
    content::RecordAction(UserMetricsAction("SelectNumberedTab"));
    browser->tab_strip_model()->ActivateTabAt(index, true);
  }
}

void SelectLastTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("SelectLastTab"));
  browser->tab_strip_model()->SelectLastTab();
}

void DuplicateTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateTabAt(browser, browser->active_index());
}

bool CanDuplicateTab(const Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  return contents && contents->GetController().GetLastCommittedEntry();
}

WebContents* DuplicateTabAt(Browser* browser, int index) {
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  CHECK(contents);
  WebContents* contents_dupe = contents->Clone();

  bool pinned = false;
  if (browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
    pinned = browser->tab_strip_model()->IsTabPinned(index);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP |
        (pinned ? TabStripModel::ADD_PINNED : 0);
    browser->tab_strip_model()->InsertWebContentsAt(
        index + 1, contents_dupe, add_types);
  } else {
    Browser* browser = NULL;
    if (browser->is_app()) {
      CHECK(!browser->is_type_popup());
      CHECK(!browser->is_type_panel());
      browser = new Browser(
          Browser::CreateParams::CreateForApp(Browser::TYPE_POPUP,
                                              browser->app_name(),
                                              gfx::Rect(),
                                              browser->profile()));
    } else if (browser->is_type_popup()) {
      browser = new Browser(
          Browser::CreateParams(Browser::TYPE_POPUP, browser->profile(),
                                browser->host_desktop_type()));
    }

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          browser->window()->GetRestoredBounds().size()));

    // We need to show the browser now.  Otherwise ContainerWin assumes the
    // WebContents is invisible and won't size it.
    browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    browser->tab_strip_model()->AddWebContents(contents_dupe, -1,
                                               content::PAGE_TRANSITION_LINK,
                                               TabStripModel::ADD_ACTIVE);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(contents_dupe, pinned);
  return contents_dupe;
}

bool CanDuplicateTabAt(Browser* browser, int index) {
  content::NavigationController& nc =
      browser->tab_strip_model()->GetWebContentsAt(index)->GetController();
  return nc.GetWebContents() && nc.GetLastCommittedEntry();
}

void ConvertPopupToTabbedBrowser(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowAsTab"));
  WebContents* contents =
      browser->tab_strip_model()->DetachWebContentsAt(browser->active_index());
  Browser* b = new Browser(Browser::CreateParams(browser->profile(),
                                                 browser->host_desktop_type()));
  b->tab_strip_model()->AppendWebContents(contents, true);
  b->window()->Show();
}

void Exit() {
  content::RecordAction(UserMetricsAction("Exit"));
  browser::AttemptUserExit();
}

void BookmarkCurrentPage(Browser* browser) {
  BookmarkCurrentPageInternal(browser, false);
}

void BookmarkCurrentPageFromStar(Browser* browser) {
  BookmarkCurrentPageInternal(browser, true);
}

bool CanBookmarkCurrentPage(const Browser* browser) {
  BookmarkModel* model =
      BookmarkModelFactory::GetForProfile(browser->profile());
  return browser_defaults::bookmarks_enabled &&
      browser->profile()->GetPrefs()->GetBoolean(
          prefs::kEditBookmarksEnabled) &&
      model && model->IsLoaded() && browser->is_type_tabbed();
}

void BookmarkAllTabs(Browser* browser) {
  chrome::ShowBookmarkAllTabsDialog(browser);
}

bool CanBookmarkAllTabs(const Browser* browser) {
  return browser->tab_strip_model()->count() > 1 &&
             CanBookmarkCurrentPage(browser);
}

void TogglePagePinnedToStartScreen(Browser* browser) {
#if defined(OS_WIN)
  MetroPinTabHelper::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())->
          TogglePinnedToStartScreen();
#endif
}

void SavePage(Browser* browser) {
  content::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (current_tab && current_tab->GetContentsMimeType() == "application/pdf")
    content::RecordAction(UserMetricsAction("PDF.SavePage"));
  current_tab->OnSavePage();
}

bool CanSavePage(const Browser* browser) {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(
      prefs::kAllowFileSelectionDialogs)) {
    return false;
  }
  return !browser->is_devtools() &&
      !(GetContentRestrictions(browser) & content::CONTENT_RESTRICTION_SAVE);
}

void ShowFindBar(Browser* browser) {
  browser->GetFindBarController()->Show();
}

void ShowPageInfo(Browser* browser,
                  content::WebContents* web_contents,
                  const GURL& url,
                  const SSLStatus& ssl,
                  bool show_history) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebsiteSettings)) {
    browser->window()->ShowPageInfo(web_contents, url, ssl, show_history);
  } else {
    browser->window()->ShowWebsiteSettings(
        profile, web_contents, url, ssl, show_history);
  }
}

void ShowChromeToMobileBubble(Browser* browser) {
  // Only show the bubble if the window is active, otherwise we may get into
  // weird situations where the bubble is deleted as soon as it is shown.
  if (browser->window()->IsActive())
    browser->window()->ShowChromeToMobileBubble();
}

void Print(Browser* browser) {
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  if (browser->profile()->GetPrefs()->GetBoolean(
      prefs::kPrintPreviewDisabled))
    print_view_manager->PrintNow();
  else
    print_view_manager->PrintPreviewNow();
}

bool CanPrint(const Browser* browser) {
  // Do not print when printing is disabled via pref or policy.
  // Do not print when a constrained window is showing. It's confusing.
  // Do not print if instant extended API is enabled and mode is NTP.
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
      !(IsShowingWebContentsModalDialog(browser) ||
      GetContentRestrictions(browser) & content::CONTENT_RESTRICTION_PRINT ||
      IsNTPModeForInstantExtendedAPI(browser));
}

void AdvancedPrint(Browser* browser) {
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  print_view_manager->AdvancedPrintNow();
}

bool CanAdvancedPrint(const Browser* browser) {
  // If printing is not disabled via pref or policy, it is always possible to
  // advanced print when the print preview is visible.
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
      (PrintPreviewShowing(browser) || CanPrint(browser));
}

void PrintToDestination(Browser* browser) {
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  print_view_manager->PrintToDestination();
}

void EmailPageLocation(Browser* browser) {
  content::RecordAction(UserMetricsAction("EmailPageLocation"));
  WebContents* wc = browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(wc);

  std::string title = net::EscapeQueryParamValue(
      UTF16ToUTF8(wc->GetTitle()), false);
  std::string page_url = net::EscapeQueryParamValue(wc->GetURL().spec(), false);
  std::string mailto = std::string("mailto:?subject=Fwd:%20") +
      title + "&body=%0A%0A" + page_url;
  platform_util::OpenExternal(GURL(mailto));
}

bool CanEmailPageLocation(const Browser* browser) {
  return browser->toolbar_model()->ShouldDisplayURL() &&
      browser->tab_strip_model()->GetActiveWebContents()->GetURL().is_valid();
}

void Cut(Browser* browser) {
  content::RecordAction(UserMetricsAction("Cut"));
  browser->window()->Cut();
}

void Copy(Browser* browser) {
  content::RecordAction(UserMetricsAction("Copy"));
  browser->window()->Copy();
}

void Paste(Browser* browser) {
  content::RecordAction(UserMetricsAction("Paste"));
  browser->window()->Paste();
}

void Find(Browser* browser) {
  content::RecordAction(UserMetricsAction("Find"));
  FindInPage(browser, false, false);
}

void FindNext(Browser* browser) {
  content::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(browser, true, true);
}

void FindPrevious(Browser* browser) {
  content::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(browser, true, false);
}

void FindInPage(Browser* browser, bool find_next, bool forward_direction) {
  ShowFindBar(browser);
  if (find_next) {
    string16 find_text;
#if defined(OS_MACOSX)
    // We always want to search for the contents of the find pasteboard on OS X.
    find_text = GetFindPboardText();
#endif
    FindTabHelper::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents())->
            StartFinding(find_text,
                         forward_direction,
                         false);  // Not case sensitive.
  }
}

void Zoom(Browser* browser, content::PageZoom zoom) {
  if (browser->is_devtools())
    return;

  chrome_page_zoom::Zoom(browser->tab_strip_model()->GetActiveWebContents(),
                         zoom);
}

void FocusToolbar(Browser* browser) {
  content::RecordAction(UserMetricsAction("FocusToolbar"));
  browser->window()->FocusToolbar();
}

void FocusLocationBar(Browser* browser) {
  content::RecordAction(UserMetricsAction("FocusLocation"));
  browser->window()->SetFocusToLocationBar(true);
}

void FocusSearch(Browser* browser) {
  // TODO(beng): replace this with FocusLocationBar
  content::RecordAction(UserMetricsAction("FocusSearch"));
  browser->window()->GetLocationBar()->FocusSearch();
}

void FocusAppMenu(Browser* browser) {
  content::RecordAction(UserMetricsAction("FocusAppMenu"));
  browser->window()->FocusAppMenu();
}

void FocusBookmarksToolbar(Browser* browser) {
  content::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  browser->window()->FocusBookmarksToolbar();
}

void FocusNextPane(Browser* browser) {
  content::RecordAction(UserMetricsAction("FocusNextPane"));
  browser->window()->RotatePaneFocus(true);
}

void FocusPreviousPane(Browser* browser) {
  content::RecordAction(UserMetricsAction("FocusPreviousPane"));
  browser->window()->RotatePaneFocus(false);
}

void ToggleDevToolsWindow(Browser* browser, DevToolsToggleAction action) {
  if (action == DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE)
    content::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  else
    content::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));
  DevToolsWindow::ToggleDevToolsWindow(browser, action);
}

bool CanOpenTaskManager() {
#if defined(OS_WIN)
  // In metro we can't display the task manager, as it is a native window.
  return !win8::IsSingleWindowMetroMode();
#else
  return true;
#endif
}

void OpenTaskManager(Browser* browser, bool highlight_background_resources) {
  content::RecordAction(UserMetricsAction("TaskManager"));
  if (highlight_background_resources)
    browser->window()->ShowBackgroundPages(browser->host_desktop_type());
  else
    browser->window()->ShowTaskManager(browser->host_desktop_type());
}

void OpenFeedbackDialog(Browser* browser) {
  content::RecordAction(UserMetricsAction("Feedback"));
  chrome::ShowFeedbackPage(browser, std::string(), std::string());
}

void ToggleBookmarkBar(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  browser->window()->ToggleBookmarkBar();
}

void ShowAppMenu(Browser* browser) {
  // We record the user metric for this event in WrenchMenu::RunMenu.
  browser->window()->ShowAppMenu();
}

void ShowAvatarMenu(Browser* browser) {
  browser->window()->ShowAvatarBubbleFromAvatarButton();
}

void OpenUpdateChromeDialog(Browser* browser) {
  content::RecordAction(UserMetricsAction("UpdateChrome"));
  browser->window()->ShowUpdateChromeDialog();
}

void ToggleSpeechInput(Browser* browser) {
  browser->tab_strip_model()->GetActiveWebContents()->
      GetRenderViewHost()->ToggleSpeechInput();
}

bool CanRequestTabletSite(WebContents* current_tab) {
  if (!current_tab)
    return false;
  return current_tab->GetController().GetActiveEntry() != NULL;
}

bool IsRequestingTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return false;
  content::NavigationEntry* entry =
      current_tab->GetController().GetActiveEntry();
  if (!entry)
    return false;
  return entry->GetIsOverridingUserAgent();
}

void ToggleRequestTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return;
  NavigationController& controller = current_tab->GetController();
  NavigationEntry* entry = controller.GetActiveEntry();
  if (!entry)
    return;
  if (entry->GetIsOverridingUserAgent()) {
    entry->SetIsOverridingUserAgent(false);
  } else {
    entry->SetIsOverridingUserAgent(true);
    current_tab->SetUserAgentOverride(
        webkit_glue::BuildUserAgentFromOSAndProduct(
            kOsOverrideForTabletSite,
            content::GetContentClient()->GetProduct()));
  }
  controller.ReloadOriginalRequestURL(true);
}

void ToggleFullscreenMode(Browser* browser) {
  browser->fullscreen_controller()->ToggleFullscreenMode();
}

void ClearCache(Browser* browser) {
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(browser->profile());
  remover->Remove(BrowsingDataRemover::REMOVE_CACHE,
                  BrowsingDataHelper::UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

bool IsDebuggerAttachedToCurrentTab(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  return contents ?
      content::DevToolsAgentHost::IsDebuggerAttached(contents) : false;
}

void ViewSource(Browser* browser, WebContents* contents) {
  DCHECK(contents);

  // Use the last committed entry, since the pending entry hasn't loaded yet and
  // won't be copied into the cloned tab.
  NavigationEntry* entry = contents->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  ViewSource(browser, contents, entry->GetURL(), entry->GetContentState());
}

void ViewSource(Browser* browser,
                WebContents* contents,
                const GURL& url,
                const std::string& content_state) {
  content::RecordAction(UserMetricsAction("ViewSource"));
  DCHECK(contents);

  // Note that Clone does not copy the pending or transient entries, so the
  // active entry in view_source_contents will be the last committed entry.
  WebContents* view_source_contents = contents->Clone();
  view_source_contents->GetController().PruneAllButActive();
  NavigationEntry* active_entry =
      view_source_contents->GetController().GetActiveEntry();
  if (!active_entry)
    return;

  GURL view_source_url = GURL(kViewSourceScheme + std::string(":") +
      url.spec());
  active_entry->SetVirtualURL(view_source_url);

  // Do not restore scroller position.
  active_entry->SetContentState(
      webkit_glue::RemoveScrollOffsetFromHistoryState(content_state));

  // Do not restore title, derive it from the url.
  active_entry->SetTitle(string16());

  // Now show view-source entry.
  if (browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP;
    browser->tab_strip_model()->InsertWebContentsAt(
        index + 1,
        view_source_contents,
        add_types);
  } else {
    Browser* b = new Browser(
        Browser::CreateParams(Browser::TYPE_TABBED, browser->profile(),
                              browser->host_desktop_type()));

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = b->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          browser->window()->GetRestoredBounds().size()));

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // WebContents is invisible and won't size it.
    b->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    b->tab_strip_model()->AddWebContents(view_source_contents, -1,
                                         content::PAGE_TRANSITION_LINK,
                                         TabStripModel::ADD_ACTIVE);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(view_source_contents, false);
}

void ViewSelectedSource(Browser* browser) {
  ViewSource(browser, browser->tab_strip_model()->GetActiveWebContents());
}

bool CanViewSource(const Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents()->
      GetController().CanViewSource();
}

void CreateApplicationShortcuts(Browser* browser) {
  content::RecordAction(UserMetricsAction("CreateShortcut"));
  extensions::TabHelper::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())->
          CreateApplicationShortcuts();
}

bool CanCreateApplicationShortcuts(const Browser* browser) {
  return extensions::TabHelper::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())->
          CanCreateApplicationShortcuts();
}

void ConvertTabToAppWindow(Browser* browser,
                           content::WebContents* contents) {
  const GURL& url = contents->GetController().GetActiveEntry()->GetURL();
  std::string app_name = web_app::GenerateApplicationNameFromURL(url);

  int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  if (index >= 0)
    browser->tab_strip_model()->DetachWebContentsAt(index);

  Browser* app_browser = new Browser(
      Browser::CreateParams::CreateForApp(
          Browser::TYPE_POPUP, app_name, gfx::Rect(), browser->profile()));
  app_browser->tab_strip_model()->AppendWebContents(contents, true);

  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->GetRenderViewHost()->SyncRendererPrefs();
  app_browser->window()->Show();
}

}  // namespace chrome
