// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_

#include <vector>

#include "base/string16.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class BookmarkNode;
class Browser;
class GURL;
class PrefService;
class Profile;

namespace content {
class BrowserContext;
class PageNavigator;
class WebContents;
}

namespace chrome {

// Number of bookmarks we'll open before prompting the user to see if they
// really want to open all.
//
// NOTE: treat this as a const. It is not const so unit tests can change the
// value.
extern int num_bookmark_urls_before_prompting;

// Opens all the bookmarks in |nodes| that are of type url and all the child
// bookmarks that are of type url for folders in |nodes|. |initial_disposition|
// dictates how the first URL is opened, all subsequent URLs are opened as
// background tabs. |navigator| is used to open the URLs.
void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition,
             content::BrowserContext* browser_context);

// Convenience for OpenAll() with a single BookmarkNode.
void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const BookmarkNode* node,
             WindowOpenDisposition initial_disposition,
             content::BrowserContext* browser_context);

// Asks the user before deleting a non-empty bookmark folder.
bool ConfirmDeleteBookmarkNode(const BookmarkNode* node,
                               gfx::NativeWindow window);

// Shows the bookmark all tabs dialog.
void ShowBookmarkAllTabsDialog(Browser* browser);

// Returns true if OpenAll() can open at least one bookmark of type url
// in |selection|.
bool HasBookmarkURLs(const std::vector<const BookmarkNode*>& selection);

// Returns true if OpenAll() can open at least one bookmark of type url
// in |selection| with incognito mode.
bool HasBookmarkURLsAllowedInIncognitoMode(
    const std::vector<const BookmarkNode*>& selection,
    content::BrowserContext* browser_context);

// Fills in the URL and title for a bookmark of |web_contents|.
void GetURLAndTitleToBookmark(content::WebContents* web_contents,
                              GURL* url,
                              string16* title);

// Toggles whether the bookmark bar is shown only on the new tab page or on
// all tabs. This is a preference modifier, not a visual modifier.
void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context);

// Returns a formatted version of |url| appropriate to display to a user with
// the given |prefs|, which may be NULL.  When re-parsing this URL, clients
// should call URLFixerUpper::FixupURL().
string16 FormatBookmarkURLForDisplay(const GURL& url,
                                     const PrefService* prefs);

// Returns whether the Apps shortcut is enabled. If true, then the visibility
// of the Apps shortcut should be controllable via an item in the bookmark
// context menu.
bool IsAppsShortcutEnabled(const Profile* profile);

// Returns true if the Apps shortcut should be displayed in the bookmark bar.
bool ShouldShowAppsShortcutInBookmarkBar(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_H_
