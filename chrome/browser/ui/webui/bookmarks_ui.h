// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_UI_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

// This class provides the source for chrome://bookmarks/
class BookmarksUIHTMLSource : public content::URLDataSource {
 public:
  BookmarksUIHTMLSource();

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;

 private:
  ~BookmarksUIHTMLSource() override;

  DISALLOW_COPY_AND_ASSIGN(BookmarksUIHTMLSource);
};

// This class is used to hook up chrome://bookmarks/ which in turn gets
// overridden by an extension.
class BookmarksUI : public content::WebUIController {
 public:
  explicit BookmarksUI(content::WebUI* web_ui);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarksUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_UI_H_
