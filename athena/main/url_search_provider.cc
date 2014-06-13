// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/url_search_provider.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/search_result.h"
#include "url/gurl.h"

namespace athena {

namespace {

class UrlSearchResult : public app_list::SearchResult {
 public:
  UrlSearchResult(content::BrowserContext* browser_context,
                  const base::string16& query)
      : browser_context_(browser_context), url_(query) {
    set_title(query);
    app_list::SearchResult::Tags title_tags;
    title_tags.push_back(app_list::SearchResult::Tag(
        app_list::SearchResult::Tag::URL, 0, query.size()));
    set_title_tags(title_tags);
    set_id(base::UTF16ToUTF8(query));
  }

 private:
  // Overriddenn from app_list::SearchResult:
  virtual void Open(int event_flags) OVERRIDE {
    ActivityManager::Get()->AddActivity(
        ActivityFactory::Get()->CreateWebActivity(browser_context_, url_));
  }

  content::BrowserContext* browser_context_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(UrlSearchResult);
};

}  // namespace

UrlSearchProvider::UrlSearchProvider(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

UrlSearchProvider::~UrlSearchProvider() {
}

void UrlSearchProvider::Start(const base::string16& query) {
  ClearResults();
  Add(scoped_ptr<app_list::SearchResult>(
      new UrlSearchResult(browser_context_, query)));
}

void UrlSearchProvider::Stop() {
}

}  // namespace athena
