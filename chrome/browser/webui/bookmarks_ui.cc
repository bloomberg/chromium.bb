// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/bookmarks_ui.h"

#include "base/message_loop.h"
#include "base/ref_counted_memory.h"
#include "base/singleton.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

////////////////////////////////////////////////////////////////////////////////
//
// BookmarksUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

BookmarksUIHTMLSource::BookmarksUIHTMLSource()
    : DataSource(chrome::kChromeUIBookmarksHost, MessageLoop::current()) {
}

void BookmarksUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_off_the_record,
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

BookmarksUI::BookmarksUI(TabContents* contents) : WebUI(contents) {
  BookmarksUIHTMLSource* html_source = new BookmarksUIHTMLSource();

  // Set up the chrome://bookmarks/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
RefCountedMemory* BookmarksUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_BOOKMARKS_FAVICON);
}
