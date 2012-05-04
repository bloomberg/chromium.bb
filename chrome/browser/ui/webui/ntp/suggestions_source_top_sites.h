// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_TOP_SITES_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_TOP_SITES_H_
#pragma once

#include <deque>

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
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
  virtual int GetWeight() OVERRIDE;
  virtual int GetItemCount() OVERRIDE;
  virtual base::DictionaryValue* PopItem() OVERRIDE;
  virtual void FetchItems(Profile* profile) OVERRIDE;
  virtual void SetCombiner(SuggestionsCombiner* combiner) OVERRIDE;

  void OnSuggestionsURLsAvailable(
      CancelableRequestProvider::Handle handle,
      const history::FilteredURLList& data);

 private:
  // Our combiner.
  SuggestionsCombiner* combiner_;

  // Keep the results of the db query here.
  std::deque<base::DictionaryValue*> items_;

  CancelableRequestConsumer history_consumer_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSourceTopSites);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_TOP_SITES_H_
