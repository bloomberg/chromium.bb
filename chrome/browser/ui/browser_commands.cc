// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_MACOSX)
#include "ui/base/cocoa/find_pasteboard.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::SSLStatus;
using content::UserMetricsAction;
using content::WebContents;

namespace chrome {
namespace {

WebContents* GetOrCloneTabForDisposition(Browser* browser,
                                         WindowOpenDisposition disposition) {
  TabContents* current_tab = chrome::GetActiveTabContents(browser);
  switch (disposition) {
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB: {
      current_tab = current_tab->Clone();
      browser->tab_strip_model()->AddTabContents(
          current_tab, -1, content::PAGE_TRANSITION_LINK,
          disposition == NEW_FOREGROUND_TAB ? TabStripModel::ADD_ACTIVE :
                                              TabStripModel::ADD_NONE);
      break;
    }
    case NEW_WINDOW: {
      current_tab = current_tab->Clone();
      Browser* b = Browser::Create(browser->profile());
      b->tab_strip_model()->AddTabContents(
          current_tab, -1, content::PAGE_TRANSITION_LINK,
          TabStripModel::ADD_ACTIVE);
      b->window()->Show();
      break;
    }
    default:
      break;
  }
  return current_tab->web_contents();
}

void ReloadInternal(Browser* browser,
                    WindowOpenDisposition disposition,
                    bool ignore_cache) {
  // If we are showing an interstitial, treat this as an OpenURL.
  WebContents* current_tab = chrome::GetActiveWebContents(browser);
  if (current_tab && current_tab->ShowingInterstitialPage()) {
    NavigationEntry* entry = current_tab->GetController().GetActiveEntry();
    DCHECK(entry);  // Should exist if interstitial is showing.
    browser->OpenURL(OpenURLParams(
        entry->GetURL(), Referrer(), disposition,
        content::PAGE_TRANSITION_RELOAD, false));
    return;
  }

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

bool HasConstrainedWindow(const Browser* browser) {
  TabContents* tab_contents = GetActiveTabContents(browser);
  return tab_contents && tab_contents->constrained_window_tab_helper()->
      constrained_window_count();
}

bool PrintPreviewShowing(const Browser* browser) {
  TabContents* contents = GetActiveTabContents(browser);
  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  return controller && (controller->GetPrintPreviewForTab(contents) ||
                        controller->is_creating_print_preview_tab());
}

bool IsNTPModeForInstantExtendedAPI(const Browser* browser) {
  return browser->search_model() &&
      chrome::search::IsInstantExtendedAPIEnabled(browser->profile()) &&
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
  WebContents* current_tab = GetActiveWebContents(browser);
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

void NewEmptyWindow(Profile* profile) {
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
    OpenEmptyWindow(profile->GetOffTheRecordProfile());
  } else {
    content::RecordAction(UserMetricsAction("NewWindow"));
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile->GetOriginalProfile());
    if (!session_service ||
        !session_service->RestoreIfNecessary(std::vector<GURL>())) {
      OpenEmptyWindow(profile->GetOriginalProfile());
    }
  }
}

Browser* OpenEmptyWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->AddBlankTab(true);
  browser->window()->Show();
  return browser;
}

void OpenWindowWithRestoredTabs(Profile* profile) {
  TabRestoreService* service = TabRestoreServiceFactory::GetForProfile(profile);
  if (service)
    service->RestoreMostRecentEntry(NULL);
}

void OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  Browser* browser = browser::FindOrCreateTabbedBrowser(
      profile->GetOffTheRecordProfile());
  AddSelectedTabWithURL(browser, url, content::PAGE_TRANSITION_LINK);
  browser->window()->Show();
}

bool CanGoBack(const Browser* browser) {
  return GetActiveWebContents(browser)->GetController().CanGoBack();
}

void GoBack(Browser* browser, WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Back"));

  TabContents* current_tab = GetActiveTabContents(browser);
  if (CanGoBack(browser)) {
    WebContents* new_tab = GetOrCloneTabForDisposition(browser, disposition);
    // If we are on an interstitial page and clone the tab, it won't be copied
    // to the new tab, so we don't need to go back.
    if (current_tab->web_contents()->ShowingInterstitialPage() &&
        (new_tab != current_tab->web_contents()))
      return;
    new_tab->GetController().GoBack();
  }
}

bool CanGoForward(const Browser* browser) {
  return GetActiveWebContents(browser)->GetController().CanGoForward();
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
  browser->OpenURL(OpenURLParams(
      browser->profile()->GetHomePage(), Referrer(), disposition,
      content::PageTransitionFromInt(
          content::PAGE_TRANSITION_AUTO_BOOKMARK |
          content::PAGE_TRANSITION_HOME_PAGE),
      false));
}

void OpenCurrentURL(Browser* browser) {
  content::RecordAction(UserMetricsAction("LoadURL"));
  LocationBar* location_bar = browser->window()->GetLocationBar();
  if (!location_bar)
    return;

  WindowOpenDisposition open_disposition =
      location_bar->GetWindowOpenDisposition();
  if (browser->OpenInstant(open_disposition))
    return;

  GURL url(location_bar->GetInputString());

  chrome::NavigateParams params(browser, url,
                                location_bar->GetPageTransition());
  params.disposition = open_disposition;
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      TabStripModel::ADD_FORCE_INDEX | TabStripModel::ADD_INHERIT_OPENER;
  chrome::Navigate(&params);

  DCHECK(browser->profile()->GetExtensionService());
  if (browser->profile()->GetExtensionService()->IsInstalledApp(url)) {
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_LOCATION);
  }
}

void Stop(Browser* browser) {
  content::RecordAction(UserMetricsAction("Stop"));
  GetActiveWebContents(browser)->Stop();
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
    browser->AddBlankTab(true);
    GetActiveWebContents(browser)->GetView()->RestoreFocus();
  } else {
    Browser* b = browser::FindOrCreateTabbedBrowser(browser->profile());
    b->AddBlankTab(true);
    b->window()->Show();
    // The call to AddBlankTab above did not set the focus to the tab as its
    // window was not active, so we have to do it explicitly.
    // See http://crbug.com/6380.
    chrome::GetActiveWebContents(b)->GetView()->RestoreFocus();
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
  if (index < browser->tab_count()) {
    content::RecordAction(UserMetricsAction("SelectNumberedTab"));
    ActivateTabAt(browser, index, true);
  }
}

void SelectLastTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("SelectLastTab"));
  browser->tab_strip_model()->SelectLastTab();
}

void DuplicateTab(Browser* browser) {
  content::RecordAction(UserMetricsAction("Duplicate"));
  browser->DuplicateContentsAt(browser->active_index());
}

bool CanDuplicateTab(const Browser* browser) {
  WebContents* contents = GetActiveWebContents(browser);
  return contents && contents->GetController().GetLastCommittedEntry();
}

void ConvertPopupToTabbedBrowser(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowAsTab"));
  TabContents* contents =
      browser->tab_strip_model()->DetachTabContentsAt(browser->active_index());
  Browser* b = Browser::Create(browser->profile());
  b->tab_strip_model()->AppendTabContents(contents, true);
  b->window()->Show();
}

void Exit() {
  content::RecordAction(UserMetricsAction("Exit"));
  browser::AttemptUserExit();
}

void BookmarkCurrentPage(Browser* browser) {
  content::RecordAction(UserMetricsAction("Star"));

  BookmarkModel* model = browser->profile()->GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  GURL url;
  string16 title;
  TabContents* tab = GetActiveTabContents(browser);
  bookmark_utils::GetURLAndTitleToBookmark(tab->web_contents(), &url, &title);
  bool was_bookmarked = model->IsBookmarked(url);
  if (!was_bookmarked && browser->profile()->IsOffTheRecord()) {
    // If we're incognito the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    tab->favicon_tab_helper()->SaveFavicon();
  }
  bookmark_utils::AddIfNotBookmarked(model, url, title);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (browser->window()->IsActive() && model->IsBookmarked(url)) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations where the bubble is deleted as soon as it is shown.
    browser->window()->ShowBookmarkBubble(url, was_bookmarked);
  }
}

bool CanBookmarkCurrentPage(const Browser* browser) {
  BookmarkModel* model = browser->profile()->GetBookmarkModel();
  return browser_defaults::bookmarks_enabled &&
      browser->profile()->GetPrefs()->GetBoolean(
          prefs::kEditBookmarksEnabled) &&
      model && model->IsLoaded() && browser->is_type_tabbed();
}

void BookmarkAllTabs(Browser* browser) {
  BookmarkEditor::ShowBookmarkAllTabsDialog(browser);
}

bool CanBookmarkAllTabs(const Browser* browser) {
  return browser->tab_count() > 1 && CanBookmarkCurrentPage(browser);
}

#if !defined(OS_WIN)
void PinCurrentPageToStartScreen(Browser* browser) {
}
#endif

void SavePage(Browser* browser) {
  content::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab = GetActiveWebContents(browser);
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
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebsiteSettings)) {
    browser->window()->ShowWebsiteSettings(
        profile, tab_contents, url, ssl, show_history);
  } else {
    browser->window()->ShowPageInfo(web_contents, url, ssl, show_history);
  }
}

void ShowChromeToMobileBubble(Browser* browser) {
  // Only show the bubble if the window is active, otherwise we may get into
  // weird situations where the bubble is deleted as soon as it is shown.
  if (browser->window()->IsActive())
    browser->window()->ShowChromeToMobileBubble();
}

void Print(Browser* browser) {
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kPrintPreviewDisabled)) {
    GetActiveTabContents(browser)->print_view_manager()->PrintNow();
  } else {
    GetActiveTabContents(browser)->print_view_manager()->
        PrintPreviewNow();
  }
}

bool CanPrint(const Browser* browser) {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(prefs::kPrintingEnabled)) {
    return false;
  }

  // Do not print when a constrained window is showing. It's confusing.
  // Do not print if instant extended API is enabled and mode is NTP.
  return !(HasConstrainedWindow(browser) ||
      GetContentRestrictions(browser) & content::CONTENT_RESTRICTION_PRINT ||
      IsNTPModeForInstantExtendedAPI(browser));
}

void AdvancedPrint(Browser* browser) {
  GetActiveTabContents(browser)->print_view_manager()->
      AdvancedPrintNow();
}

bool CanAdvancedPrint(const Browser* browser) {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(prefs::kPrintingEnabled)) {
    return false;
  }

  // It is always possible to advanced print when print preview is visible.
  return PrintPreviewShowing(browser) || CanPrint(browser);
}

void PrintToDestination(Browser* browser) {
  GetActiveTabContents(browser)->print_view_manager()->PrintToDestination();
}

void EmailPageLocation(Browser* browser) {
  content::RecordAction(UserMetricsAction("EmailPageLocation"));
  WebContents* wc = GetActiveWebContents(browser);
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
      GetActiveWebContents(browser)->GetURL().is_valid();
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
    GetActiveTabContents(browser)->
        find_tab_helper()->StartFinding(find_text,
                                        forward_direction,
                                        false);  // Not case sensitive.
  }
}

void Zoom(Browser* browser, content::PageZoom zoom) {
  if (browser->is_devtools())
    return;

  content::RenderViewHost* host =
      GetActiveWebContents(browser)->GetRenderViewHost();
  if (zoom == content::PAGE_ZOOM_RESET) {
    host->SetZoomLevel(0);
    content::RecordAction(UserMetricsAction("ZoomNormal"));
    return;
  }

  double current_zoom_level = GetActiveWebContents(browser)->GetZoomLevel();
  double default_zoom_level =
      browser->profile()->GetPrefs()->GetDouble(prefs::kDefaultZoomLevel);

  // Generate a vector of zoom levels from an array of known presets along with
  // the default level added if necessary.
  std::vector<double> zoom_levels =
      chrome_page_zoom::PresetZoomLevels(default_zoom_level);

  if (zoom == content::PAGE_ZOOM_OUT) {
    // Iterate through the zoom levels in reverse order to find the next
    // lower level based on the current zoom level for this page.
    for (std::vector<double>::reverse_iterator i = zoom_levels.rbegin();
         i != zoom_levels.rend(); ++i) {
      double zoom_level = *i;
      if (content::ZoomValuesEqual(zoom_level, current_zoom_level))
        continue;
      if (zoom_level < current_zoom_level) {
        host->SetZoomLevel(zoom_level);
        content::RecordAction(UserMetricsAction("ZoomMinus"));
        return;
      }
      content::RecordAction(UserMetricsAction("ZoomMinus_AtMinimum"));
    }
  } else {
    // Iterate through the zoom levels in normal order to find the next
    // higher level based on the current zoom level for this page.
    for (std::vector<double>::const_iterator i = zoom_levels.begin();
         i != zoom_levels.end(); ++i) {
      double zoom_level = *i;
      if (content::ZoomValuesEqual(zoom_level, current_zoom_level))
        continue;
      if (zoom_level > current_zoom_level) {
        host->SetZoomLevel(zoom_level);
        content::RecordAction(UserMetricsAction("ZoomPlus"));
        return;
      }
    }
    content::RecordAction(UserMetricsAction("ZoomPlus_AtMaximum"));
  }
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

  DevToolsWindow::ToggleDevToolsWindow(
      GetActiveWebContents(browser)->GetRenderViewHost(),
      action);
}

bool CanOpenTaskManager() {
#if defined(OS_WIN)
  // In metro we can't display the task manager, as it is a native window.
  return !base::win::IsMetroProcess();
#else
  return true;
#endif
}

void OpenTaskManager(Browser* browser, bool highlight_background_resources) {
  content::RecordAction(UserMetricsAction("TaskManager"));
  if (highlight_background_resources)
    browser->window()->ShowBackgroundPages();
  else
    browser->window()->ShowTaskManager();
}

void OpenFeedbackDialog(Browser* browser) {
  content::RecordAction(UserMetricsAction("Feedback"));
  browser::ShowWebFeedbackView(browser, std::string(), std::string());
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
  GetActiveWebContents(browser)->GetRenderViewHost()->ToggleSpeechInput();
}

void ViewSource(Browser* browser, TabContents* contents) {
  DCHECK(contents);

  NavigationEntry* active_entry =
    contents->web_contents()->GetController().GetActiveEntry();
  if (!active_entry)
    return;

  ViewSource(browser, contents, active_entry->GetURL(),
             active_entry->GetContentState());
}

void ViewSource(Browser* browser,
                TabContents* contents,
                const GURL& url,
                const std::string& content_state) {
  content::RecordAction(UserMetricsAction("ViewSource"));
  DCHECK(contents);

  TabContents* view_source_contents = contents->Clone();
  view_source_contents->web_contents()->GetController().PruneAllButActive();
  NavigationEntry* active_entry =
      view_source_contents->web_contents()->GetController().GetActiveEntry();
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
    int index = browser->tab_strip_model()->GetIndexOfTabContents(contents);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP;
    browser->tab_strip_model()->InsertTabContentsAt(index + 1,
                                                    view_source_contents,
                                                    add_types);
  } else {
    Browser* b = Browser::CreateWithParams(
        Browser::CreateParams(Browser::TYPE_TABBED, browser->profile()));

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = b->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          browser->window()->GetRestoredBounds().size()));

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // WebContents is invisible and won't size it.
    b->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    chrome::AddTab(b, view_source_contents, content::PAGE_TRANSITION_LINK);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(view_source_contents, false);
}

void ViewSelectedSource(Browser* browser) {
  ViewSource(browser, chrome::GetActiveTabContents(browser));
}

bool CanViewSource(const Browser* browser) {
  return chrome::GetActiveWebContents(browser)->GetController().CanViewSource();
}

void CreateApplicationShortcuts(Browser* browser) {
  content::RecordAction(UserMetricsAction("CreateShortcut"));
  chrome::GetActiveTabContents(browser)->extension_tab_helper()->
      CreateApplicationShortcuts();
}

bool CanCreateApplicationShortcuts(const Browser* browser) {
  return chrome::GetActiveTabContents(browser)->extension_tab_helper()->
      CanCreateApplicationShortcuts();
}

void ConvertTabToAppWindow(Browser* browser,
                           content::WebContents* contents) {
  const GURL& url = contents->GetController().GetActiveEntry()->GetURL();
  std::string app_name = web_app::GenerateApplicationNameFromURL(url);

  int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  if (index >= 0)
    browser->tab_strip_model()->DetachTabContentsAt(index);

  Browser* app_browser = Browser::CreateWithParams(
      Browser::CreateParams::CreateForApp(
          Browser::TYPE_POPUP, app_name, gfx::Rect(), browser->profile()));
  TabContents* tab_contents = TabContents::FromWebContents(contents);
  if (!tab_contents)
    tab_contents = new TabContents(contents);
  app_browser->tab_strip_model()->AppendTabContents(tab_contents, true);

  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->GetRenderViewHost()->SyncRendererPrefs();
  app_browser->window()->Show();
}

}  // namespace chrome
