// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "googleurl/src/gurl.h"

using content::WebContents;

////////////////////////////////////////////////////////////////////////////////
//
// BookmarksUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

BookmarksUIHTMLSource::BookmarksUIHTMLSource()
    : DataSource(chrome::kChromeUIBookmarksHost, MessageLoop::current()) {
}

void BookmarksUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";

  SendResponse(request_id, NULL);
}

std::string BookmarksUIHTMLSource::GetMimeType(const std::string& path) const {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// BookmarksUI
//
////////////////////////////////////////////////////////////////////////////////

BookmarksUI::BookmarksUI(WebContents* contents) : ChromeWebUI(contents) {
  BookmarksUIHTMLSource* html_source = new BookmarksUIHTMLSource();

  // Set up the chrome://bookmarks/ source.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
RefCountedMemory* BookmarksUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_BOOKMARKS_FAVICON);
}

////////////////////////////////////////////////////////////////////////////////
//
// BookmarkEditor
//
////////////////////////////////////////////////////////////////////////////////

// static
void BookmarkEditor::ShowWebUI(Profile* profile,
                               const EditDetails& details) {
  int64 editId = 0;
  if (details.type == EditDetails::EXISTING_NODE) {
    DCHECK(details.existing_node);
    editId = details.existing_node->id();
  } else if (details.type == EditDetails::NEW_URL) {
    DCHECK(details.parent_node);
    // Add a new bookmark with the title/URL of the current tab.
    GURL bmUrl;
    string16 bmTitle;
    bookmark_utils::GetURLAndTitleToBookmarkFromCurrentTab(profile, &bmUrl,
        &bmTitle);
    BookmarkModel* bm = profile->GetBookmarkModel();
    const BookmarkNode* newNode = bm->AddURL(details.parent_node,
        details.parent_node->child_count(), bmTitle, bmUrl);

    // Just edit this bookmark like for editing an existing node.
    // TODO(rbyers): This is to be replaced with a WebUI dialog to prevent
    // the context switch to a different tab.
    editId = newNode->id();
  } else {
    NOTREACHED() << "Unhandled bookmark edit details type";
  }

  GURL url = GURL(chrome::kChromeUIBookmarksURL).Resolve(StringPrintf("/#e=%s",
      base::Int64ToString(editId).c_str()));

  // Invoke the WebUI bookmark editor to edit the specified bookmark.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  DCHECK(browser);
  browser::NavigateParams params(
      browser->GetSingletonTabNavigateParams(url));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser->ShowSingletonTabOverwritingNTP(params);
}
