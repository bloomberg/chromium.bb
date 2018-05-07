// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace app_list {
class AppContextMenu;
class TokenizedString;
class TokenizedStringMatch;
}  // namespace app_list

// ChromeSearchResult consists of an icon, title text and details text. Title
// and details text can have tagged ranges that are displayed differently from
// default style.
class ChromeSearchResult : public app_list::SearchResult {
 public:
  ChromeSearchResult();

  const std::string& comparable_id() const { return comparable_id_; }

  // TODO(mukai): Remove this method and really simplify the ownership of
  // SearchResult. Ideally, SearchResult will be copyable.
  virtual std::unique_ptr<ChromeSearchResult> Duplicate() const = 0;

  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }
  AppListModelUpdater* model_updater() const { return model_updater_; }

  double relevance() const { return relevance_; }
  void set_relevance(double relevance) { relevance_ = relevance; }

  // Updates the result's relevance score, and sets its title and title tags,
  // based on a string match result.
  void UpdateFromMatch(const app_list::TokenizedString& title,
                       const app_list::TokenizedStringMatch& match);

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install). |callback| takes the ownership
  // of the returned menu model.
  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::MenuModel>)>;
  virtual void GetContextMenuModel(GetMenuModelCallback callback);

  // Invoked when a context menu item of this search result is selected.
  void ContextMenuItemSelected(int command_id, int event_flags);

  static std::string TagsDebugStringForTest(const std::string& text,
                                            const Tags& tags);

 protected:
  void set_comparable_id(const std::string& comparable_id) {
    comparable_id_ = comparable_id;
  }

  // Get the context menu of a certain search result. This could be different
  // for different kinds of items.
  virtual app_list::AppContextMenu* GetAppContextMenu();

 private:
  // ID that can be compared across results from different providers to remove
  // duplicates. May be empty, in which case |id_| will be used for comparison.
  std::string comparable_id_;

  // The relevance of this result to the search, which is determined by the
  // search query. It's used for sorting when we publish the results to the
  // SearchModel in Ash. We'll update metadata_->display_score based on the
  // sorted order, group multiplier and group boost.
  double relevance_ = 0;

  AppListModelUpdater* model_updater_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeSearchResult);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
