// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/unguessable_token.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

namespace ui {
class MenuModel;
}

namespace app_list {

class SearchResultObserver;
class TokenizedString;
class TokenizedStringMatch;

// SearchResult consists of an icon, title text and details text. Title and
// details text can have tagged ranges that are displayed differently from
// default style.
class APP_LIST_MODEL_EXPORT SearchResult {
 public:
  using ResultType = ash::SearchResultType;
  using DisplayType = ash::SearchResultDisplayType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;

  SearchResult();
  virtual ~SearchResult();

  const gfx::ImageSkia& icon() const { return icon_; }
  void SetIcon(const gfx::ImageSkia& icon);

  const gfx::ImageSkia& badge_icon() const { return badge_icon_; }
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);

  const base::string16& title() const { return title_; }
  void set_title(const base::string16& title) { title_ = title; }

  const Tags& title_tags() const { return title_tags_; }
  void set_title_tags(const Tags& tags) { title_tags_ = tags; }

  const base::string16& details() const { return details_; }
  void set_details(const base::string16& details) { details_ = details; }

  const Tags& details_tags() const { return details_tags_; }
  void set_details_tags(const Tags& tags) { details_tags_ = tags; }

  float rating() const { return rating_; }
  void SetRating(float rating);

  const base::string16& formatted_price() const { return formatted_price_; }
  void SetFormattedPrice(const base::string16& formatted_price);

  const base::UnguessableToken& answer_card_contents_token() const {
    return answer_card_contents_token_;
  }
  void set_answer_card_contents_token(const base::UnguessableToken& token) {
    answer_card_contents_token_ = token;
  }

  const std::string& id() const { return id_; }
  const std::string& comparable_id() const { return comparable_id_; }

  double relevance() const { return relevance_; }
  void set_relevance(double relevance) { relevance_ = relevance; }

  DisplayType display_type() const { return display_type_; }
  void set_display_type(DisplayType display_type) {
    display_type_ = display_type;
  }

  ResultType result_type() const { return result_type_; }
  void set_result_type(ResultType result_type) { result_type_ = result_type; }

  int distance_from_origin() { return distance_from_origin_; }
  void set_distance_from_origin(int distance) {
    distance_from_origin_ = distance;
  }

  const Actions& actions() const { return actions_; }
  void SetActions(const Actions& sets);

  // Whether the result can be automatically selected by a voice query.
  // (Non-voice results can still appear in the results list to be manually
  // selected.)
  bool voice_result() const { return voice_result_; }

  bool is_installing() const { return is_installing_; }
  void SetIsInstalling(bool is_installing);

  int percent_downloaded() const { return percent_downloaded_; }
  void SetPercentDownloaded(int percent_downloaded);

  bool is_omnibox_search() const { return is_omnibox_search_; }
  void set_is_omnibox_search(bool is_omnibox_search) {
    is_omnibox_search_ = is_omnibox_search;
  }

  void NotifyItemInstalled();

  void AddObserver(SearchResultObserver* observer);
  void RemoveObserver(SearchResultObserver* observer);

  // Updates the result's relevance score, and sets its title and title tags,
  // based on a string match result.
  void UpdateFromMatch(const TokenizedString& title,
                       const TokenizedStringMatch& match);

  // TODO(mukai): Remove this method and really simplify the ownership of
  // SearchResult. Ideally, SearchResult will be copyable.
  virtual std::unique_ptr<SearchResult> Duplicate() const = 0;

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index, int event_flags);

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install).
  // Note the returned menu model is owned by this item.
  virtual ui::MenuModel* GetContextMenuModel();

  // Returns a string showing |text| marked up with brackets indicating the
  // tag positions in |tags|. Useful for debugging and testing.
  static std::string TagsDebugString(const std::string& text, const Tags& tags);

 protected:
  void set_id(const std::string& id) { id_ = id; }
  void set_comparable_id(const std::string& comparable_id) {
    comparable_id_ = comparable_id;
  }
  void set_voice_result(bool voice_result) { voice_result_ = voice_result; }

 private:
  friend class SearchController;

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags);

  gfx::ImageSkia icon_;
  gfx::ImageSkia badge_icon_;

  base::string16 title_;
  Tags title_tags_;

  base::string16 details_;
  Tags details_tags_;

  // Amount of the app's stars in play store. Not exist if set to negative.
  float rating_ = -1.0f;

  // Formatted price label of the app in play store. Not exist if set to empty.
  base::string16 formatted_price_;

  // A token used to show answer card contents. When answer card provider and ui
  // runs in the same process (i.e classic ash, or mash before UI migration),
  // AnswerCardContensRegistry could be used to map the token to a view.
  // Otherwise (mash after UI migration), the token is used to embed the answer
  // card contents.
  base::UnguessableToken answer_card_contents_token_;

  std::string id_;
  // ID that can be compared across results from different providers to remove
  // duplicates. May be empty, in which case |id_| will be used for comparison.
  std::string comparable_id_;
  double relevance_ = 0;
  DisplayType display_type_ = ash::SearchResultDisplayType::kList;

  ResultType result_type_ = ash::SearchResultType::kUnknown;

  // The Manhattan distance from the origin of all search results to this
  // result. This is logged for UMA.
  int distance_from_origin_ = -1;

  Actions actions_;
  bool voice_result_ = false;

  bool is_installing_ = false;
  int percent_downloaded_ = 0;

  // Indicates whether result is an omnibox non-url search result. Set by
  // OmniboxResult subclass.
  bool is_omnibox_search_ = false;

  base::ObserverList<SearchResultObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchResult);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
