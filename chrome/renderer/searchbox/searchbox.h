// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/common/instant_restricted_id_cache.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
class RenderView;
}

class SearchBox : public content::RenderViewObserver,
                  public content::RenderViewObserverTracker<SearchBox> {
 public:
  explicit SearchBox(content::RenderView* render_view);
  virtual ~SearchBox();

  // Sends ChromeViewHostMsg_LogEvent to the browser.
  void LogEvent(NTPLoggingEventType event);

  // Sends ChromeViewHostMsg_LogMostVisitedImpression to the browser.
  void LogMostVisitedImpression(int position, const base::string16& provider);

  // Sends ChromeViewHostMsg_LogMostVisitedNavigation to the browser.
  void LogMostVisitedNavigation(int position, const base::string16& provider);

  // Sends ChromeViewHostMsg_ChromeIdentityCheck to the browser.
  void CheckIsUserSignedInToChromeAs(const base::string16& identity);

  // Sends ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem to the browser.
  void DeleteMostVisitedItem(InstantRestrictedID most_visited_item_id);

  // Generates the favicon URL of the most visited item specified by the
  // |transient_url|. If the |transient_url| is valid, returns true and fills in
  // |url|. If the |transient_url| is invalid, returns true and |url| is set to
  // "chrome-search://favicon/" in order to prevent the invalid URL to be
  // requested.
  //
  // Valid forms of |transient_url|:
  //    chrome-search://favicon/<view_id>/<restricted_id>
  //    chrome-search://favicon/<favicon_parameters>/<view_id>/<restricted_id>
  bool GenerateFaviconURLFromTransientURL(const GURL& transient_url,
                                          GURL* url) const;

  // Generates the thumbnail URL of the most visited item specified by the
  // |transient_url|. If the |transient_url| is valid, returns true and fills in
  // |url|. If the |transient_url| is invalid, returns false  and |url| is not
  // set.
  //
  // Valid form of |transient_url|:
  //    chrome-search://thumb/<render_view_id>/<most_visited_item_id>
  bool GenerateThumbnailURLFromTransientURL(const GURL& transient_url,
                                            GURL* url) const;

  // Returns the latest most visited items sent by the browser.
  void GetMostVisitedItems(
      std::vector<InstantMostVisitedItemIDPair>* items) const;

  // If the |most_visited_item_id| is found in the cache, sets |item| to it
  // and returns true.
  bool GetMostVisitedItemWithID(InstantRestrictedID most_visited_item_id,
                                InstantMostVisitedItem* item) const;

  // Sends ChromeViewHostMsg_FocusOmnibox to the browser.
  void Focus();

  // Sends ChromeViewHostMsg_SearchBoxNavigate to the browser.
  void NavigateToURL(const GURL& url,
                     WindowOpenDisposition disposition,
                     bool is_most_visited_item_url);

  // Sends ChromeViewHostMsg_SearchBoxPaste to the browser.
  void Paste(const base::string16& text);

  const ThemeBackgroundInfo& GetThemeBackgroundInfo();

  // Sends ChromeViewHostMsg_SetVoiceSearchSupported to the browser.
  void SetVoiceSearchSupported(bool supported);

  // Sends ChromeViewHostMsg_StartCapturingKeyStrokes to the browser.
  void StartCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_StopCapturingKeyStrokes to the browser.
  void StopCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions to the
  // browser.
  void UndoAllMostVisitedDeletions();

  // Sends ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion to the browser.
  void UndoMostVisitedDeletion(InstantRestrictedID most_visited_item_id);

  bool app_launcher_enabled() const { return app_launcher_enabled_; }
  bool is_focused() const { return is_focused_; }
  bool is_input_in_progress() const { return is_input_in_progress_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }
  bool display_instant_results() const { return display_instant_results_; }
  const base::string16& query() const { return query_; }
  int start_margin() const { return start_margin_; }
  const InstantSuggestion& suggestion() const { return suggestion_; }

 private:
  // Overridden from content::RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSetPageSequenceNumber(int page_seq_no);
  void OnChromeIdentityCheckResult(const base::string16& identity,
                                   bool identity_match);
  void OnDetermineIfPageSupportsInstant();
  void OnFocusChanged(OmniboxFocusState new_focus_state,
                      OmniboxFocusChangeReason reason);
  void OnMarginChange(int margin);
  void OnMostVisitedChanged(
      const std::vector<InstantMostVisitedItem>& items);
  void OnPromoInformationReceived(bool is_app_launcher_enabled);
  void OnSetDisplayInstantResults(bool display_instant_results);
  void OnSetInputInProgress(bool input_in_progress);
  void OnSetSuggestionToPrefetch(const InstantSuggestion& suggestion);
  void OnSubmit(const base::string16& query);
  void OnThemeChanged(const ThemeBackgroundInfo& theme_info);
  void OnToggleVoiceSearch();

  // Returns the current zoom factor of the render view or 1 on failure.
  double GetZoom() const;

  // Sets the searchbox values to their initial value.
  void Reset();

  // Returns the URL of the Most Visited item specified by the |item_id|.
  GURL GetURLForMostVisitedItem(InstantRestrictedID item_id) const;

  int page_seq_no_;
  bool app_launcher_enabled_;
  bool is_focused_;
  bool is_input_in_progress_;
  bool is_key_capture_enabled_;
  bool display_instant_results_;
  InstantRestrictedIDCache<InstantMostVisitedItem> most_visited_items_cache_;
  ThemeBackgroundInfo theme_info_;
  base::string16 query_;
  int start_margin_;
  InstantSuggestion suggestion_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
