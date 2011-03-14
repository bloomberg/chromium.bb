// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
#pragma once

#include <string>

#include "chrome/common/content_settings_types.h"
#include "ui/gfx/native_widget_types.h"

// This file contains functions for running a variety of browser dialogs and
// popups. The dialogs here are the ones that the caller does not need to
// access the class of the popup. It allows us to break dependencies by
// allowing the callers to not depend on the classes implementing the dialogs.
// TODO: Make as many of these methods as possible cross platform, and move them
// into chrome/browser/ui/browser_dialogs.h.

class Browser;
class BrowserView;
class EditSearchEngineControllerDelegate;
class Extension;
class FilePath;
class FindBar;
class GURL;
class InfoBubbleDelegate;
class Profile;
class TabContents;
class TemplateURL;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class Widget;
class Window;
}  // namespace views

namespace browser {

// Shows the "Report a problem with this page" dialog box. See BugReportView.
void ShowBugReportView(views::Window* parent,
                       Profile* profile,
                       TabContents* tab);

// Shows the "Clear browsing data" dialog box. See ClearBrowsingDataView.
void ShowClearBrowsingDataView(gfx::NativeWindow parent,
                               Profile* profile);

// Shows or hides the global bookmark bubble for the star button.
void ShowBookmarkBubbleView(views::Window* parent,
                            const gfx::Rect& bounds,
                            InfoBubbleDelegate* delegate,
                            Profile* profile,
                            const GURL& url,
                            bool newly_bookmarked);
void HideBookmarkBubbleView();
bool IsBookmarkBubbleViewShowing();

// Shows the bookmark manager.
void ShowBookmarkManagerView(Profile* profile);

// Shows the about dialog. See AboutChromeView.
views::Window* ShowAboutChromeView(gfx::NativeWindow parent,
                                   Profile* profile);

// Creates and returns a find bar for the given browser window. See FindBarWin.
FindBar* CreateFindBar(BrowserView* browser_view);

// Shows the keyword editor. See KeywordEditorView.
void ShowKeywordEditorView(Profile* profile);

// Shows the Task Manager.
void ShowTaskManager();

// Shows the Task Manager, highlighting the background pages.
void ShowBackgroundPages();

#if defined(OS_CHROMEOS)
// Shows the Login Wizard.
void ShowLoginWizard(const std::string& start_screen, const gfx::Size& size);
#endif

// Shows a dialog box that allows a search engine to be edited. |template_url|
// is the search engine being edited. If it is NULL, then the dialog will add a
// new search engine with the data the user supplies. |delegate| is an object
// to be notified when the user is done editing, or NULL. If NULL, the dialog
// will update the model with the user's edits directly.
void EditSearchEngine(gfx::NativeWindow parent,
                      const TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile);

// Shows the repost form confirmation dialog box.
void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents);

// Shows the collected cookies dialog box.
void ShowCollectedCookiesDialog(gfx::NativeWindow parent_window,
                                TabContents* tab_contents);


// Shows the create web app shortcut dialog box.
void ShowCreateWebAppShortcutsDialog(gfx::NativeWindow parent_window,
                                     TabContents* tab_contents);

// Shows the create chrome app shortcut dialog box.
void ShowCreateChromeAppShortcutsDialog(gfx::NativeWindow parent_window,
                                        Profile* profile,
                                        const Extension* app);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
