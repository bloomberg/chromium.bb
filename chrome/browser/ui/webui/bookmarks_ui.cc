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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "googleurl/src/gurl.h"


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

BookmarksUI::BookmarksUI(TabContents* contents) : ChromeWebUI(contents) {
  BookmarksUIHTMLSource* html_source = new BookmarksUIHTMLSource();

  // Set up the chrome://bookmarks/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
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
  GURL url(chrome::kChromeUIBookmarksURL);
  if (details.type == EditDetails::EXISTING_NODE) {
    DCHECK(details.existing_node);
    url = url.Resolve(StringPrintf("/#e=%s",
        base::Int64ToString(details.existing_node->id()).c_str()));
  } else if (details.type == EditDetails::NEW_URL) {
    DCHECK(details.parent_node);
    url = url.Resolve(StringPrintf("/#a=%s",
        base::Int64ToString(details.parent_node->id()).c_str()));
  } else {
    NOTREACHED() << "Unhandled bookmark edit details type";
  }
  // Get parent browser object.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  DCHECK(browser);
  browser::NavigateParams params(
      browser->GetSingletonTabNavigateParams(url));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser->ShowSingletonTabOverwritingNTP(params);
}
