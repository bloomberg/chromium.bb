// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_performance.h"

namespace {

// The size of the InstantMostVisitedItem cache.
const size_t kMaxInstantMostVisitedItemCacheSize = 100;

// Returns true if items stored in |old_item_id_pairs| and |new_items| are
// equal.
bool AreMostVisitedItemsEqual(
    const std::vector<InstantMostVisitedItemIDPair>& old_item_id_pairs,
    const std::vector<InstantMostVisitedItem>& new_items) {
  if (old_item_id_pairs.size() != new_items.size())
    return false;

  for (size_t i = 0; i < new_items.size(); ++i) {
    if (new_items[i].url != old_item_id_pairs[i].second.url ||
        new_items[i].title != old_item_id_pairs[i].second.title ||
        new_items[i].source != old_item_id_pairs[i].second.source) {
      return false;
    }
  }
  return true;
}

// Helper for SearchBox::GenerateImageURLFromTransientURL().
class SearchBoxIconURLHelper: public SearchBox::IconURLHelper {
 public:
  explicit SearchBoxIconURLHelper(const SearchBox* search_box);
  ~SearchBoxIconURLHelper() override;
  int GetViewID() const override;
  std::string GetURLStringFromRestrictedID(InstantRestrictedID rid) const
      override;

 private:
  const SearchBox* search_box_;
};

SearchBoxIconURLHelper::SearchBoxIconURLHelper(const SearchBox* search_box)
    : search_box_(search_box) {
}

SearchBoxIconURLHelper::~SearchBoxIconURLHelper() {
}

int SearchBoxIconURLHelper::GetViewID() const {
  return search_box_->render_frame()->GetRenderView()->GetRoutingID();
}

std::string SearchBoxIconURLHelper::GetURLStringFromRestrictedID(
    InstantRestrictedID rid) const {
  InstantMostVisitedItem item;
  if (!search_box_->GetMostVisitedItemWithID(rid, &item))
    return std::string();

  return item.url.spec();
}

}  // namespace

namespace internal {  // for testing

// Parses "<view_id>/<restricted_id>". If successful, assigns
// |*view_id| := "<view_id>", |*rid| := "<restricted_id>", and returns true.
bool ParseViewIdAndRestrictedId(const std::string& id_part,
                                int* view_id_out,
                                InstantRestrictedID* rid_out) {
  DCHECK(view_id_out);
  DCHECK(rid_out);
  // Check that the path is of Most visited item ID form.
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      id_part, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() != 2)
    return false;

  int view_id;
  InstantRestrictedID rid;
  if (!base::StringToInt(tokens[0], &view_id) || view_id < 0 ||
      !base::StringToInt(tokens[1], &rid) || rid < 0)
    return false;

  *view_id_out = view_id;
  *rid_out = rid;
  return true;
}

// Takes a favicon |url| that looks like:
//
//   chrome-search://favicon/<view_id>/<restricted_id>
//   chrome-search://favicon/<parameters>/<view_id>/<restricted_id>
//
// If successful, assigns |*param_part| := "" or "<parameters>/" (note trailing
// slash), |*view_id| := "<view_id>", |*rid| := "rid", and returns true.
bool ParseIconRestrictedUrl(const GURL& url,
                            std::string* param_part,
                            int* view_id,
                            InstantRestrictedID* rid) {
  DCHECK(param_part);
  DCHECK(view_id);
  DCHECK(rid);
  // Strip leading slash.
  std::string raw_path = url.path();
  DCHECK_GT(raw_path.length(), (size_t) 0);
  DCHECK_EQ(raw_path[0], '/');
  raw_path = raw_path.substr(1);

  // Get the starting index of the page URL.
  chrome::ParsedFaviconPath parsed;
  if (!chrome::ParseFaviconPath(raw_path, &parsed))
    return false;
  int path_index = parsed.path_index;

  std::string id_part = raw_path.substr(path_index);
  if (!ParseViewIdAndRestrictedId(id_part, view_id, rid))
    return false;

  *param_part = raw_path.substr(0, path_index);
  return true;
}

void TranslateIconRestrictedUrl(const GURL& transient_url,
                                const SearchBox::IconURLHelper& helper,
                                GURL* url) {
  std::string params;
  int view_id = -1;
  InstantRestrictedID rid = -1;

  if (!internal::ParseIconRestrictedUrl(transient_url, &params, &view_id,
                                        &rid) ||
      view_id != helper.GetViewID()) {
    *url = GURL(base::StringPrintf("chrome-search://%s/",
                                   chrome::kChromeUIFaviconHost));
  } else {
    std::string item_url = helper.GetURLStringFromRestrictedID(rid);
    *url = GURL(base::StringPrintf("chrome-search://%s/%s%s",
                                   chrome::kChromeUIFaviconHost, params.c_str(),
                                   item_url.c_str()));
  }
}

std::string FixupAndValidateUrl(const std::string& url) {
  GURL gurl = url_formatter::FixupURL(url, /*desired_tld=*/std::string());
  if (!gurl.is_valid())
    return std::string();

  // Unless "http" was specified, replaces FixupURL's default "http" with
  // "https".
  if (url.find(std::string("http://")) == std::string::npos &&
      gurl.SchemeIs(url::kHttpScheme)) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpsScheme);
    gurl = gurl.ReplaceComponents(replacements);
  }

  return gurl.spec();
}

}  // namespace internal

SearchBox::IconURLHelper::IconURLHelper() = default;

SearchBox::IconURLHelper::~IconURLHelper() = default;

SearchBox::SearchBox(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SearchBox>(render_frame),
      binding_(this),
      can_run_js_in_renderframe_(false),
      page_seq_no_(0),
      is_focused_(false),
      is_input_in_progress_(false),
      is_key_capture_enabled_(false),
      most_visited_items_cache_(kMaxInstantMostVisitedItemCacheSize),
      has_received_most_visited_(false),
      weak_ptr_factory_(this) {
  // Connect to the embedded search interface in the browser.
  chrome::mojom::EmbeddedSearchConnectorAssociatedPtr connector;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&connector);
  chrome::mojom::EmbeddedSearchClientAssociatedPtrInfo embedded_search_client;
  binding_.Bind(mojo::MakeRequest(&embedded_search_client));
  connector->Connect(mojo::MakeRequest(&embedded_search_service_),
                     std::move(embedded_search_client));
}

SearchBox::~SearchBox() = default;

void SearchBox::LogEvent(NTPLoggingEventType event) {
  base::Time navigation_start = base::Time::FromDoubleT(
      render_frame()->GetWebFrame()->Performance().NavigationStart());
  base::Time now = base::Time::Now();
  base::TimeDelta delta = now - navigation_start;
  embedded_search_service_->LogEvent(page_seq_no_, event, delta);
}

void SearchBox::LogSuggestionEventWithValue(
    NTPSuggestionsLoggingEventType event,
    int data) {
  base::Time navigation_start = base::Time::FromDoubleT(
      render_frame()->GetWebFrame()->Performance().NavigationStart());
  base::Time now = base::Time::Now();
  base::TimeDelta delta = now - navigation_start;
  embedded_search_service_->LogSuggestionEventWithValue(page_seq_no_, event,
                                                        data, delta);
}

void SearchBox::LogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  embedded_search_service_->LogMostVisitedImpression(page_seq_no_, impression);
}

void SearchBox::LogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  embedded_search_service_->LogMostVisitedNavigation(page_seq_no_, impression);
}

void SearchBox::CheckIsUserSignedInToChromeAs(const base::string16& identity) {
  embedded_search_service_->ChromeIdentityCheck(
      page_seq_no_, identity,
      base::BindOnce(&SearchBox::ChromeIdentityCheckResult,
                     weak_ptr_factory_.GetWeakPtr(), identity));
}

void SearchBox::CheckIsUserSyncingHistory() {
  embedded_search_service_->HistorySyncCheck(
      page_seq_no_, base::BindOnce(&SearchBox::HistorySyncCheckResult,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void SearchBox::DeleteMostVisitedItem(
    InstantRestrictedID most_visited_item_id) {
  GURL url = GetURLForMostVisitedItem(most_visited_item_id);
  if (!url.is_valid())
    return;
  embedded_search_service_->DeleteMostVisitedItem(page_seq_no_, url);
}

void SearchBox::GenerateImageURLFromTransientURL(const GURL& transient_url,
                                                 GURL* url) const {
  SearchBoxIconURLHelper helper(this);
  internal::TranslateIconRestrictedUrl(transient_url, helper, url);
}

void SearchBox::GetMostVisitedItems(
    std::vector<InstantMostVisitedItemIDPair>* items) const {
  most_visited_items_cache_.GetCurrentItems(items);
}

bool SearchBox::AreMostVisitedItemsAvailable() const {
  return has_received_most_visited_;
}

bool SearchBox::GetMostVisitedItemWithID(
    InstantRestrictedID most_visited_item_id,
    InstantMostVisitedItem* item) const {
  return most_visited_items_cache_.GetItemWithRestrictedID(most_visited_item_id,
                                                           item);
}

const ThemeBackgroundInfo& SearchBox::GetThemeBackgroundInfo() const {
  return theme_info_;
}

void SearchBox::Paste(const base::string16& text) {
  embedded_search_service_->PasteAndOpenDropdown(page_seq_no_, text);
}

void SearchBox::StartCapturingKeyStrokes() {
  embedded_search_service_->FocusOmnibox(page_seq_no_, OMNIBOX_FOCUS_INVISIBLE);
}

void SearchBox::StopCapturingKeyStrokes() {
  embedded_search_service_->FocusOmnibox(page_seq_no_, OMNIBOX_FOCUS_NONE);
}

void SearchBox::UndoAllMostVisitedDeletions() {
  embedded_search_service_->UndoAllMostVisitedDeletions(page_seq_no_);
}

void SearchBox::UndoMostVisitedDeletion(
    InstantRestrictedID most_visited_item_id) {
  GURL url = GetURLForMostVisitedItem(most_visited_item_id);
  if (!url.is_valid())
    return;
  embedded_search_service_->UndoMostVisitedDeletion(page_seq_no_, url);
}

bool SearchBox::IsCustomLinks() const {
  return is_custom_links_;
}

void SearchBox::ToggleMostVisitedOrCustomLinks() {
  embedded_search_service_->ToggleMostVisitedOrCustomLinks(page_seq_no_);
}

void SearchBox::AddCustomLink(const GURL& url, const std::string& title) {
  if (!url.is_valid()) {
    AddCustomLinkResult(false);
    return;
  }
  embedded_search_service_->AddCustomLink(
      page_seq_no_, url, title,
      base::BindOnce(&SearchBox::AddCustomLinkResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchBox::UpdateCustomLink(InstantRestrictedID link_id,
                                 const GURL& new_url,
                                 const std::string& new_title) {
  GURL url = GetURLForMostVisitedItem(link_id);
  if (!url.is_valid()) {
    UpdateCustomLinkResult(false);
    return;
  }
  embedded_search_service_->UpdateCustomLink(
      page_seq_no_, url, new_url, new_title,
      base::BindOnce(&SearchBox::UpdateCustomLinkResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchBox::ReorderCustomLink(InstantRestrictedID link_id, int new_pos) {
  GURL url = GetURLForMostVisitedItem(link_id);
  if (!url.is_valid())
    return;
  embedded_search_service_->ReorderCustomLink(page_seq_no_, url, new_pos);
}

void SearchBox::DeleteCustomLink(InstantRestrictedID most_visited_item_id) {
  GURL url = GetURLForMostVisitedItem(most_visited_item_id);
  if (!url.is_valid()) {
    DeleteCustomLinkResult(false);
    return;
  }
  embedded_search_service_->DeleteCustomLink(
      page_seq_no_, url,
      base::BindOnce(&SearchBox::DeleteCustomLinkResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchBox::UndoCustomLinkAction() {
  embedded_search_service_->UndoCustomLinkAction(page_seq_no_);
}

void SearchBox::ResetCustomLinks() {
  embedded_search_service_->ResetCustomLinks(page_seq_no_);
}

std::string SearchBox::FixupAndValidateUrl(const std::string& url) const {
  return internal::FixupAndValidateUrl(url);
}

void SearchBox::SetCustomBackgroundURL(const GURL& background_url) {
  embedded_search_service_->SetCustomBackgroundURL(background_url);
}

void SearchBox::SetCustomBackgroundURLWithAttributions(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  embedded_search_service_->SetCustomBackgroundURLWithAttributions(
      background_url, attribution_line_1, attribution_line_2, action_url);
}

void SearchBox::SelectLocalBackgroundImage() {
  embedded_search_service_->SelectLocalBackgroundImage();
}

void SearchBox::BlocklistSearchSuggestion(int task_version, long task_id) {
  embedded_search_service_->BlocklistSearchSuggestion(task_version, task_id);
}

void SearchBox::BlocklistSearchSuggestionWithHash(
    int task_version,
    long task_id,
    const std::vector<uint8_t>& hash) {
  embedded_search_service_->BlocklistSearchSuggestionWithHash(task_version,
                                                              task_id, hash);
}

void SearchBox::SearchSuggestionSelected(int task_version,
                                         long task_id,
                                         const std::vector<uint8_t>& hash) {
  embedded_search_service_->SearchSuggestionSelected(task_version, task_id,
                                                     hash);
}

void SearchBox::OptOutOfSearchSuggestions() {
  embedded_search_service_->OptOutOfSearchSuggestions();
}

void SearchBox::SetPageSequenceNumber(int page_seq_no) {
  page_seq_no_ = page_seq_no;
}

void SearchBox::ChromeIdentityCheckResult(const base::string16& identity,
                                          bool identity_match) {
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchChromeIdentityCheckResult(
        render_frame()->GetWebFrame(), identity, identity_match);
  }
}

void SearchBox::FocusChanged(OmniboxFocusState new_focus_state,
                             OmniboxFocusChangeReason reason) {
  bool key_capture_enabled = new_focus_state == OMNIBOX_FOCUS_INVISIBLE;
  if (key_capture_enabled != is_key_capture_enabled_) {
    // Tell the page if the key capture mode changed unless the focus state
    // changed because of TYPING. This is because in that case, the browser
    // hasn't really stopped capturing key strokes.
    //
    // (More practically, if we don't do this check, the page would receive
    // onkeycapturechange before the corresponding onchange, and the page would
    // have no way of telling whether the keycapturechange happened because of
    // some actual user action or just because they started typing.)
    if (reason != OMNIBOX_FOCUS_CHANGE_TYPING) {
      is_key_capture_enabled_ = key_capture_enabled;
      DVLOG(1) << render_frame() << " KeyCaptureChange";
      if (can_run_js_in_renderframe_) {
        SearchBoxExtension::DispatchKeyCaptureChange(
            render_frame()->GetWebFrame());
      }
    }
  }
  bool is_focused = new_focus_state == OMNIBOX_FOCUS_VISIBLE;
  if (is_focused != is_focused_) {
    is_focused_ = is_focused;
    DVLOG(1) << render_frame() << " FocusChange";
    if (can_run_js_in_renderframe_)
      SearchBoxExtension::DispatchFocusChange(render_frame()->GetWebFrame());
  }
}

void SearchBox::HistorySyncCheckResult(bool sync_history) {
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchHistorySyncCheckResult(
        render_frame()->GetWebFrame(), sync_history);
  }
}

void SearchBox::AddCustomLinkResult(bool success) {
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchAddCustomLinkResult(
        render_frame()->GetWebFrame(), success);
  }
}

void SearchBox::UpdateCustomLinkResult(bool success) {
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchUpdateCustomLinkResult(
        render_frame()->GetWebFrame(), success);
  }
}

void SearchBox::DeleteCustomLinkResult(bool success) {
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchDeleteCustomLinkResult(
        render_frame()->GetWebFrame(), success);
  }
}

void SearchBox::MostVisitedChanged(
    const std::vector<InstantMostVisitedItem>& items,
    bool is_custom_links) {
  has_received_most_visited_ = true;
  is_custom_links_ = is_custom_links;

  std::vector<InstantMostVisitedItemIDPair> last_known_items;
  GetMostVisitedItems(&last_known_items);

  if (AreMostVisitedItemsEqual(last_known_items, items)) {
    return;  // Do not send duplicate onmostvisitedchange events.
  }

  most_visited_items_cache_.AddItems(items);
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchMostVisitedChanged(
        render_frame()->GetWebFrame());
  }
}

void SearchBox::SetInputInProgress(bool is_input_in_progress) {
  if (is_input_in_progress_ == is_input_in_progress)
    return;

  is_input_in_progress_ = is_input_in_progress;
  DVLOG(1) << render_frame() << " SetInputInProgress";
  if (can_run_js_in_renderframe_) {
    if (is_input_in_progress_)
      SearchBoxExtension::DispatchInputStart(render_frame()->GetWebFrame());
    else
      SearchBoxExtension::DispatchInputCancel(render_frame()->GetWebFrame());
  }
}

void SearchBox::ThemeChanged(const ThemeBackgroundInfo& theme_info) {
  // Do not send duplicate notifications.
  if (theme_info_ == theme_info)
    return;

  theme_info_ = theme_info;
  if (can_run_js_in_renderframe_)
    SearchBoxExtension::DispatchThemeChange(render_frame()->GetWebFrame());
}

GURL SearchBox::GetURLForMostVisitedItem(InstantRestrictedID item_id) const {
  InstantMostVisitedItem item;
  return GetMostVisitedItemWithID(item_id, &item) ? item.url : GURL();
}

void SearchBox::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                         ui::PageTransition transition) {
  can_run_js_in_renderframe_ = true;
}

void SearchBox::OnDestruct() {
  delete this;
}
