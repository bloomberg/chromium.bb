// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/favicon/favicon_url_parser.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/renderer/render_view.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

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
        new_items[i].title != old_item_id_pairs[i].second.title) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace internal {  // for testing

// Parses |path| and fills in |id| with the InstantRestrictedID obtained from
// the |path|. |render_view_id| is the ID of the associated RenderView.
//
// |path| is a pair of |render_view_id| and |restricted_id|, and it is
// contained in Instant Extended URLs. A valid |path| is in the form:
// <render_view_id>/<restricted_id>
//
// If the |path| is valid, returns true and fills in |id| with restricted_id
// value. If the |path| is invalid, returns false and |id| is not set.
bool GetInstantRestrictedIDFromPath(int render_view_id,
                                    const std::string& path,
                                    InstantRestrictedID* id) {
  // Check that the path is of Most visited item ID form.
  std::vector<std::string> tokens;
  if (Tokenize(path, "/", &tokens) != 2)
    return false;

  int view_id = 0;
  if (!base::StringToInt(tokens[0], &view_id) || view_id != render_view_id)
    return false;
  return base::StringToInt(tokens[1], id);
}

bool GetRestrictedIDFromFaviconUrl(int render_view_id,
                                   const GURL& url,
                                   std::string* favicon_params,
                                   InstantRestrictedID* rid) {
  // Strip leading slash.
  std::string raw_path = url.path();
  DCHECK_GT(raw_path.length(), (size_t) 0);
  DCHECK_EQ(raw_path[0], '/');
  raw_path = raw_path.substr(1);

  chrome::ParsedFaviconPath parsed;
  if (!chrome::ParseFaviconPath(raw_path, favicon_base::FAVICON, &parsed))
    return false;

  // The part of the URL which details the favicon parameters should be returned
  // so the favicon URL can be reconstructed, by replacing the restricted_id
  // with the actual URL from which the favicon is being requested.
  *favicon_params = raw_path.substr(0, parsed.path_index);

  // The part of the favicon URL which is supposed to contain the URL from
  // which the favicon is being requested (i.e., the page's URL) actually
  // contains a pair in the format "<view_id>/<restricted_id>". If the page's
  // URL is not in the expected format then the execution must be stopped,
  // returning |true|, indicating that the favicon URL should be translated
  // without the page's URL part, to prevent search providers from spoofing
  // the user's browsing history. For example, the following favicon URL
  // "chrome-search://favicon/http://www.secretsite.com" it is not in the
  // expected format "chrome-search://favicon/<view_id>/<restricted_id>" so
  // the pages's URL part ("http://www.secretsite.com") should be removed
  // entirely from the translated URL otherwise the search engine would know
  // if the user has visited that page (by verifying whether the favicon URL
  // returns an image for a particular page's URL); the translated URL in this
  // case would be "chrome-search://favicon/" which would simply return the
  // default favicon.
  std::string id_part = raw_path.substr(parsed.path_index);
  InstantRestrictedID id;
  if (!GetInstantRestrictedIDFromPath(render_view_id, id_part, &id))
    return true;

  *rid = id;
  return true;
}

// Parses a thumbnail |url| and fills in |id| with the InstantRestrictedID
// obtained from the |url|. |render_view_id| is the ID of the associated
// RenderView.
//
// Valid |url| forms:
// chrome-search://thumb/<view_id>/<restricted_id>
//
// If the |url| is valid, returns true and fills in |id| with restricted_id
// value. If the |url| is invalid, returns false and |id| is not set.
bool GetRestrictedIDFromThumbnailUrl(int render_view_id,
                                     const GURL& url,
                                     InstantRestrictedID* id) {
  // Strip leading slash.
  std::string path = url.path();
  DCHECK_GT(path.length(), (size_t) 0);
  DCHECK_EQ(path[0], '/');
  path = path.substr(1);

  return GetInstantRestrictedIDFromPath(render_view_id, path, id);
}

}  // namespace internal

SearchBox::SearchBox(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SearchBox>(render_view),
    page_seq_no_(0),
    app_launcher_enabled_(false),
    is_focused_(false),
    is_input_in_progress_(false),
    is_key_capture_enabled_(false),
    display_instant_results_(false),
    most_visited_items_cache_(kMaxInstantMostVisitedItemCacheSize),
    query_(),
    start_margin_(0) {
}

SearchBox::~SearchBox() {
}

void SearchBox::LogEvent(NTPLoggingEventType event) {
  render_view()->Send(new ChromeViewHostMsg_LogEvent(
      render_view()->GetRoutingID(), page_seq_no_, event));
}

void SearchBox::LogMostVisitedImpression(int position,
                                         const base::string16& provider) {
  render_view()->Send(new ChromeViewHostMsg_LogMostVisitedImpression(
      render_view()->GetRoutingID(), page_seq_no_, position, provider));
}

void SearchBox::LogMostVisitedNavigation(int position,
                                         const base::string16& provider) {
  render_view()->Send(new ChromeViewHostMsg_LogMostVisitedNavigation(
      render_view()->GetRoutingID(), page_seq_no_, position, provider));
}

void SearchBox::CheckIsUserSignedInToChromeAs(const base::string16& identity) {
  render_view()->Send(new ChromeViewHostMsg_ChromeIdentityCheck(
      render_view()->GetRoutingID(), page_seq_no_, identity));
}

void SearchBox::DeleteMostVisitedItem(
    InstantRestrictedID most_visited_item_id) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
      render_view()->GetRoutingID(),
      page_seq_no_,
      GetURLForMostVisitedItem(most_visited_item_id)));
}

bool SearchBox::GenerateFaviconURLFromTransientURL(const GURL& transient_url,
                                                   GURL* url) const {
  std::string favicon_params;
  InstantRestrictedID rid = -1;
  bool success = internal::GetRestrictedIDFromFaviconUrl(
      render_view()->GetRoutingID(), transient_url, &favicon_params, &rid);
  if (!success)
    return false;

  InstantMostVisitedItem item;
  std::string item_url;
  if (rid != -1 && GetMostVisitedItemWithID(rid, &item))
    item_url = item.url.spec();

  *url = GURL(base::StringPrintf("chrome-search://favicon/%s%s",
                                 favicon_params.c_str(),
                                 item_url.c_str()));
  return true;
}

bool SearchBox::GenerateThumbnailURLFromTransientURL(const GURL& transient_url,
                                                     GURL* url) const {
  InstantRestrictedID rid = 0;
  if (!internal::GetRestrictedIDFromThumbnailUrl(render_view()->GetRoutingID(),
                                                 transient_url, &rid)) {
    return false;
  }

  GURL most_visited_item_url(GetURLForMostVisitedItem(rid));
  if (most_visited_item_url.is_empty())
    return false;
  *url = GURL(base::StringPrintf("chrome-search://thumb/%s",
                                 most_visited_item_url.spec().c_str()));
  return true;
}

void SearchBox::GetMostVisitedItems(
    std::vector<InstantMostVisitedItemIDPair>* items) const {
  return most_visited_items_cache_.GetCurrentItems(items);
}

bool SearchBox::GetMostVisitedItemWithID(
    InstantRestrictedID most_visited_item_id,
    InstantMostVisitedItem* item) const {
  return most_visited_items_cache_.GetItemWithRestrictedID(most_visited_item_id,
                                                           item);
}

const ThemeBackgroundInfo& SearchBox::GetThemeBackgroundInfo() {
  return theme_info_;
}

void SearchBox::Focus() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), page_seq_no_, OMNIBOX_FOCUS_VISIBLE));
}

void SearchBox::NavigateToURL(const GURL& url,
                              WindowOpenDisposition disposition,
                              bool is_most_visited_item_url) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxNavigate(
      render_view()->GetRoutingID(), page_seq_no_, url,
      disposition, is_most_visited_item_url));
}

void SearchBox::Paste(const base::string16& text) {
  render_view()->Send(new ChromeViewHostMsg_PasteAndOpenDropdown(
      render_view()->GetRoutingID(), page_seq_no_, text));
}

void SearchBox::SetVoiceSearchSupported(bool supported) {
  render_view()->Send(new ChromeViewHostMsg_SetVoiceSearchSupported(
      render_view()->GetRoutingID(), page_seq_no_, supported));
}

void SearchBox::StartCapturingKeyStrokes() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), page_seq_no_, OMNIBOX_FOCUS_INVISIBLE));
}

void SearchBox::StopCapturingKeyStrokes() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), page_seq_no_, OMNIBOX_FOCUS_NONE));
}

void SearchBox::UndoAllMostVisitedDeletions() {
  render_view()->Send(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          render_view()->GetRoutingID(), page_seq_no_));
}

void SearchBox::UndoMostVisitedDeletion(
    InstantRestrictedID most_visited_item_id) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      render_view()->GetRoutingID(), page_seq_no_,
      GetURLForMostVisitedItem(most_visited_item_id)));
}

bool SearchBox::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchBox, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetPageSequenceNumber,
                        OnSetPageSequenceNumber)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_ChromeIdentityCheckResult,
                        OnChromeIdentityCheckResult)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DetermineIfPageSupportsInstant,
                        OnDetermineIfPageSupportsInstant)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxFocusChanged, OnFocusChanged)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxMarginChange, OnMarginChange)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxMostVisitedItemsChanged,
                        OnMostVisitedChanged)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxPromoInformation,
                        OnPromoInformationReceived)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSetDisplayInstantResults,
                        OnSetDisplayInstantResults)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSetInputInProgress,
                        OnSetInputInProgress)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSetSuggestionToPrefetch,
                        OnSetSuggestionToPrefetch)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSubmit, OnSubmit)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxThemeChanged,
                        OnThemeChanged)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxToggleVoiceSearch,
                        OnToggleVoiceSearch)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchBox::OnSetPageSequenceNumber(int page_seq_no) {
  page_seq_no_ = page_seq_no;
}

void SearchBox::OnChromeIdentityCheckResult(const base::string16& identity,
                                            bool identity_match) {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchChromeIdentityCheckResult(
        render_view()->GetWebView()->mainFrame(), identity, identity_match);
  }
}

void SearchBox::OnDetermineIfPageSupportsInstant() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    bool result = extensions_v8::SearchBoxExtension::PageSupportsInstant(
        render_view()->GetWebView()->mainFrame());
    DVLOG(1) << render_view() << " PageSupportsInstant: " << result;
    render_view()->Send(new ChromeViewHostMsg_InstantSupportDetermined(
        render_view()->GetRoutingID(), page_seq_no_, result));
  }
}

void SearchBox::OnFocusChanged(OmniboxFocusState new_focus_state,
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
    if (reason != OMNIBOX_FOCUS_CHANGE_TYPING &&
        render_view()->GetWebView() &&
        render_view()->GetWebView()->mainFrame()) {
      is_key_capture_enabled_ = key_capture_enabled;
      DVLOG(1) << render_view() << " OnKeyCaptureChange";
      extensions_v8::SearchBoxExtension::DispatchKeyCaptureChange(
          render_view()->GetWebView()->mainFrame());
    }
  }
  bool is_focused = new_focus_state == OMNIBOX_FOCUS_VISIBLE;
  if (is_focused != is_focused_) {
    is_focused_ = is_focused;
    DVLOG(1) << render_view() << " OnFocusChange";
    if (render_view()->GetWebView() &&
        render_view()->GetWebView()->mainFrame()) {
      extensions_v8::SearchBoxExtension::DispatchFocusChange(
          render_view()->GetWebView()->mainFrame());
    }
  }
}

void SearchBox::OnMarginChange(int margin) {
  start_margin_ = margin;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchMarginChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnMostVisitedChanged(
    const std::vector<InstantMostVisitedItem>& items) {
  std::vector<InstantMostVisitedItemIDPair> last_known_items;
  GetMostVisitedItems(&last_known_items);

  if (AreMostVisitedItemsEqual(last_known_items, items))
    return;  // Do not send duplicate onmostvisitedchange events.

  most_visited_items_cache_.AddItems(items);
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchMostVisitedChanged(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnPromoInformationReceived(bool is_app_launcher_enabled) {
  app_launcher_enabled_ = is_app_launcher_enabled;
}

void SearchBox::OnSetDisplayInstantResults(bool display_instant_results) {
  display_instant_results_ = display_instant_results;
}

void SearchBox::OnSetInputInProgress(bool is_input_in_progress) {
  if (is_input_in_progress_ != is_input_in_progress) {
    is_input_in_progress_ = is_input_in_progress;
    DVLOG(1) << render_view() << " OnSetInputInProgress";
    if (render_view()->GetWebView() &&
        render_view()->GetWebView()->mainFrame()) {
      if (is_input_in_progress_) {
        extensions_v8::SearchBoxExtension::DispatchInputStart(
            render_view()->GetWebView()->mainFrame());
      } else {
        extensions_v8::SearchBoxExtension::DispatchInputCancel(
            render_view()->GetWebView()->mainFrame());
      }
    }
  }
}

void SearchBox::OnSetSuggestionToPrefetch(const InstantSuggestion& suggestion) {
  suggestion_ = suggestion;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnSetSuggestionToPrefetch";
    extensions_v8::SearchBoxExtension::DispatchSuggestionChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnSubmit(const base::string16& query) {
  query_ = query;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnSubmit";
    extensions_v8::SearchBoxExtension::DispatchSubmit(
        render_view()->GetWebView()->mainFrame());
  }
  if (!query.empty())
    Reset();
}

void SearchBox::OnThemeChanged(const ThemeBackgroundInfo& theme_info) {
  // Do not send duplicate notifications.
  if (theme_info_ == theme_info)
    return;

  theme_info_ = theme_info;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchThemeChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnToggleVoiceSearch() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchToggleVoiceSearch(
        render_view()->GetWebView()->mainFrame());
  }
}

GURL SearchBox::GetURLForMostVisitedItem(InstantRestrictedID item_id) const {
  InstantMostVisitedItem item;
  return GetMostVisitedItemWithID(item_id, &item) ? item.url : GURL();
}

void SearchBox::Reset() {
  query_.clear();
  suggestion_ = InstantSuggestion();
  start_margin_ = 0;
  is_focused_ = false;
  is_key_capture_enabled_ = false;
  theme_info_ = ThemeBackgroundInfo();
}
