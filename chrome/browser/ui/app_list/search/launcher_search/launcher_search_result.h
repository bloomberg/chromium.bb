// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_RESULT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_icon_image_loader.h"
#include "extensions/common/extension.h"
#include "ui/app_list/search_result.h"
#include "url/gurl.h"

namespace app_list {

class LauncherSearchResult : public SearchResult,
                             public LauncherSearchIconImageLoader::Observer {
 public:
  LauncherSearchResult(
      const std::string& item_id,
      const GURL& icon_url,
      const int discrete_value_relevance,
      Profile* profile,
      const extensions::Extension* extension,
      scoped_ptr<chromeos::launcher_search_provider::ErrorReporter>
          error_reporter);
  ~LauncherSearchResult() override;
  scoped_ptr<SearchResult> Duplicate() const override;
  void Open(int event_flags) override;

  void OnIconImageChanged(LauncherSearchIconImageLoader* image_loader) override;
  void OnBadgeIconImageChanged(
      LauncherSearchIconImageLoader* image_loader) override;

 private:
  // Constructor for duplicating a result.
  LauncherSearchResult(
      const std::string& item_id,
      const int discrete_value_relevance,
      Profile* profile,
      const extensions::Extension* extension,
      const linked_ptr<LauncherSearchIconImageLoader>& icon_image_loader);
  void Initialize();

  // Returns search result ID. The search result ID is comprised of the
  // extension ID and the extension-supplied item ID. This is to avoid naming
  // collisions for results of different extensions.
  std::string GetSearchResultId();

  const std::string item_id_;
  // Must be between 0 and kMaxSearchResultScore.
  const int discrete_value_relevance_;
  Profile* profile_;
  const extensions::Extension* extension_;
  linked_ptr<LauncherSearchIconImageLoader> icon_image_loader_;

  DISALLOW_COPY_AND_ASSIGN(LauncherSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_RESULT_H_
