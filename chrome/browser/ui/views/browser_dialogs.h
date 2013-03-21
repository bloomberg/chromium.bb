// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_

#include "ui/gfx/native_widget_types.h"

// This file contains functions for running a variety of browser dialogs and
// popups. The dialogs here are the ones that the caller does not need to
// access the class of the popup. It allows us to break dependencies by
// allowing the callers to not depend on the classes implementing the dialogs.
// TODO: Make as many of these methods as possible cross platform, and move them
// into chrome/browser/ui/browser_dialogs.h.

class BookmarkBubbleViewObserver;
class Browser;
class BrowserView;
class EditSearchEngineControllerDelegate;
class FindBar;
class GURL;
class Profile;
class TemplateURL;

namespace extensions {
class Extension;
}

namespace views {
class View;
}

namespace chrome {

// Shows or hides the bookmark bubble anchored to the supplied view.
void ShowBookmarkBubbleView(views::View* anchor_view,
                            BookmarkBubbleViewObserver* observer,
                            Profile* profile,
                            const GURL& url,
                            bool newly_bookmarked);
void HideBookmarkBubbleView();
bool IsBookmarkBubbleViewShowing();

// Shows or hides the Chrome To Mobile bubble anchored to the supplied view.
void ShowChromeToMobileBubbleView(views::View* anchor_view, Browser* browser);
void HideChromeToMobileBubbleView();
bool IsChromeToMobileBubbleViewShowing();

// Creates and returns a find bar for the given browser window. See FindBarWin.
FindBar* CreateFindBar(BrowserView* browser_view);

// Shows a dialog box that allows a search engine to be edited. |template_url|
// is the search engine being edited. If it is NULL, then the dialog will add a
// new search engine with the data the user supplies. |delegate| is an object
// to be notified when the user is done editing, or NULL. If NULL, the dialog
// will update the model with the user's edits directly.
void EditSearchEngine(gfx::NativeWindow parent,
                      TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile);

// Shows the create chrome app shortcut dialog box.
void ShowCreateChromeAppShortcutsDialog(gfx::NativeWindow parent_window,
                                        Profile* profile,
                                        const extensions::Extension* app);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_BROWSER_DIALOGS_H_
