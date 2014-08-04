// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_H_

#include <string>

#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class CommandObserver;
class GURL;
class Profile;

namespace content {
class PageState;
class WebContents;
struct SSLStatus;
}

namespace chrome {

// For all commands, where a tab is not specified, the active tab is assumed.

bool IsCommandEnabled(Browser* browser, int command);
bool SupportsCommand(Browser* browser, int command);
bool ExecuteCommand(Browser* browser, int command);
bool ExecuteCommandWithDisposition(Browser* browser,
                                   int command,
                                   WindowOpenDisposition disposition);
void UpdateCommandEnabled(Browser* browser, int command, bool enabled);
void AddCommandObserver(Browser*, int command, CommandObserver* observer);
void RemoveCommandObserver(Browser*, int command, CommandObserver* observer);

int GetContentRestrictions(const Browser* browser);

// Opens a new window with the default blank tab.
void NewEmptyWindow(Profile* profile, HostDesktopType desktop_type);

// Opens a new window with the default blank tab. This bypasses metrics and
// various internal bookkeeping; NewEmptyWindow (above) is preferred.
Browser* OpenEmptyWindow(Profile* profile, HostDesktopType desktop_type);

// Opens a new window with the tabs from |profile|'s TabRestoreService.
void OpenWindowWithRestoredTabs(Profile* profile,
                                HostDesktopType host_desktop_type);

// Opens the specified URL in a new browser window in an incognito session on
// the desktop specified by |desktop_type|. If there is already an existing
// active incognito session for the specified |profile|, that session is re-
// used.
void OpenURLOffTheRecord(Profile* profile, const GURL& url,
                         chrome::HostDesktopType desktop_type);

bool CanGoBack(const Browser* browser);
void GoBack(Browser* browser, WindowOpenDisposition disposition);
bool CanGoForward(const Browser* browser);
void GoForward(Browser* browser, WindowOpenDisposition disposition);
bool NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disposition);
void Reload(Browser* browser, WindowOpenDisposition disposition);
void ReloadIgnoringCache(Browser* browser, WindowOpenDisposition disposition);
bool CanReload(const Browser* browser);
void Home(Browser* browser, WindowOpenDisposition disposition);
void OpenCurrentURL(Browser* browser);
void Stop(Browser* browser);
void NewWindow(Browser* browser);
void NewIncognitoWindow(Browser* browser);
void CloseWindow(Browser* browser);
void NewTab(Browser* browser);
void CloseTab(Browser* browser);
bool CanZoomIn(content::WebContents* contents);
bool CanZoomOut(content::WebContents* contents);
bool ActualSize(content::WebContents* contents);
void RestoreTab(Browser* browser);
TabStripModelDelegate::RestoreTabType GetRestoreTabType(
    const Browser* browser);
void SelectNextTab(Browser* browser);
void SelectPreviousTab(Browser* browser);
void MoveTabNext(Browser* browser);
void MoveTabPrevious(Browser* browser);
void SelectNumberedTab(Browser* browser, int index);
void SelectLastTab(Browser* browser);
void DuplicateTab(Browser* browser);
bool CanDuplicateTab(const Browser* browser);
content::WebContents* DuplicateTabAt(Browser* browser, int index);
bool CanDuplicateTabAt(Browser* browser, int index);
void ConvertPopupToTabbedBrowser(Browser* browser);
void Exit();
void BookmarkCurrentPage(Browser* browser);
bool CanBookmarkCurrentPage(const Browser* browser);
void BookmarkAllTabs(Browser* browser);
bool CanBookmarkAllTabs(const Browser* browser);
void Translate(Browser* browser);
void ManagePasswordsForPage(Browser* browser);
void TogglePagePinnedToStartScreen(Browser* browser);
void SavePage(Browser* browser);
bool CanSavePage(const Browser* browser);
void ShowFindBar(Browser* browser);
void ShowWebsiteSettings(Browser* browser,
                         content::WebContents* web_contents,
                         const GURL& url,
                         const content::SSLStatus& ssl);
void Print(Browser* browser);
bool CanPrint(Browser* browser);
void AdvancedPrint(Browser* browser);
bool CanAdvancedPrint(Browser* browser);
void PrintToDestination(Browser* browser);
void EmailPageLocation(Browser* browser);
bool CanEmailPageLocation(const Browser* browser);
void Cut(Browser* browser);
void Copy(Browser* browser);
void Paste(Browser* browser);
void Find(Browser* browser);
void FindNext(Browser* browser);
void FindPrevious(Browser* browser);
void FindInPage(Browser* browser, bool find_next, bool forward_direction);
void Zoom(Browser* browser, content::PageZoom zoom);
void FocusToolbar(Browser* browser);
void FocusLocationBar(Browser* browser);
void FocusSearch(Browser* browser);
void FocusAppMenu(Browser* browser);
void FocusBookmarksToolbar(Browser* browser);
void FocusInfobars(Browser* browser);
void FocusNextPane(Browser* browser);
void FocusPreviousPane(Browser* browser);
void ToggleDevToolsWindow(Browser* browser, DevToolsToggleAction action);
bool CanOpenTaskManager();
void OpenTaskManager(Browser* browser);
void OpenFeedbackDialog(Browser* browser);
void ToggleBookmarkBar(Browser* browser);
void ShowAppMenu(Browser* browser);
void ShowAvatarMenu(Browser* browser);
void OpenUpdateChromeDialog(Browser* browser);
void ToggleSpeechInput(Browser* browser);
void DistillCurrentPage(Browser* browser);
bool CanRequestTabletSite(content::WebContents* current_tab);
bool IsRequestingTabletSite(Browser* browser);
void ToggleRequestTabletSite(Browser* browser);
void ToggleFullscreenMode(Browser* browser);
void ClearCache(Browser* browser);
bool IsDebuggerAttachedToCurrentTab(Browser* browser);

// Opens a view-source tab for a given web contents.
void ViewSource(Browser* browser, content::WebContents* tab);

// Opens a view-source tab for any frame within a given web contents.
void ViewSource(Browser* browser,
                content::WebContents* tab,
                const GURL& url,
                const content::PageState& page_state);

void ViewSelectedSource(Browser* browser);
bool CanViewSource(const Browser* browser);

void CreateApplicationShortcuts(Browser* browser);
void CreateBookmarkAppFromCurrentWebContents(Browser* browser);
bool CanCreateApplicationShortcuts(const Browser* browser);
bool CanCreateBookmarkApp(const Browser* browser);

void ConvertTabToAppWindow(Browser* browser, content::WebContents* contents);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
