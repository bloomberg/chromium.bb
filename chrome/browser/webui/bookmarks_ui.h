// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_BOOKMARKS_UI_H_
#define CHROME_BROWSER_WEBUI_BOOKMARKS_UI_H_
#pragma once

#include <string>

#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "content/browser/webui/web_ui.h"

class GURL;
class RefCountedMemory;

// This class provides the source for chrome://bookmarks/
class BookmarksUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  BookmarksUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarksUIHTMLSource);
};

// This class is used to hook up chrome://bookmarks/ which in turn gets
// overridden by an extension.
class BookmarksUI : public WebUI {
 public:
  explicit BookmarksUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarksUI);
};

#endif  // CHROME_BROWSER_WEBUI_BOOKMARKS_UI_H_
