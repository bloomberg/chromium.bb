// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_TOP_SITES_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_TOP_SITES_H_

#include <deque>

#include "base/basictypes.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source.h"

class SuggestionsCombiner;
class Profile;

namespace base {
class DictionaryValue;
}

// A SuggestionsSource that uses the local TopSites database to provide
// suggestions.
class SuggestionsSourceTopSites : public SuggestionsSource {
 public:
  SuggestionsSourceTopSites();
  virtual ~SuggestionsSourceTopSites();

 protected:
  // SuggestionsSource overrides:
  virtual void SetDebug(bool enable) OVERRIDE;
  virtual int GetWeight() OVERRIDE;
  virtual int GetItemCount() OVERRIDE;
  virtual base::DictionaryValue* PopItem() OVERRIDE;
  virtual void FetchItems(Profile* profile) OVERRIDE;
  virtual void SetCombiner(SuggestionsCombiner* combiner) OVERRIDE;

  void OnSuggestionsUrlsAvailable(const history::FilteredURLList* data);

 private:

  // Gets the sorting order from the command-line arguments. Defaults to
  // |ORDER_BY_RECENCY| if there are no command-line argument specifying a
  // sorting order.
  static history::VisitFilter::SortingOrder GetSortingOrder();

  // Gets the filter width from the command-line arguments. Defaults to one
  // hour if there are no command-line argument setting the filter width.
  static base::TimeDelta GetFilterWidth();

  // Our combiner.
  SuggestionsCombiner* combiner_;

  // Keep the results of the db query here.
  std::deque<base::DictionaryValue*> items_;

  // Whether the source should provide additional debug information or not.
  bool debug_;

  base::CancelableTaskTracker history_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSourceTopSites);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_TOP_SITES_H_
