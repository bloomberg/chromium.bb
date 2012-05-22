// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_DISCOVERY_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_DISCOVERY_H_
#pragma once

#include <deque>

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source.h"
#include "net/url_request/url_fetcher_delegate.h"

class SuggestionsCombiner;
class Profile;

namespace base {
class DictionaryValue;
}

namespace net {
class URLFetcher;
}

// A source that suggests websites the user might find interesting.
class SuggestionsSourceDiscovery : public SuggestionsSource,
                                   public net::URLFetcherDelegate {
 public:
  SuggestionsSourceDiscovery();
  virtual ~SuggestionsSourceDiscovery();

  // net::URLFetcherDelegate override and implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  // SuggestionsSource overrides:
  virtual int GetWeight() OVERRIDE;
  virtual int GetItemCount() OVERRIDE;
  virtual base::DictionaryValue* PopItem() OVERRIDE;
  virtual void FetchItems(Profile* profile) OVERRIDE;
  virtual void SetCombiner(SuggestionsCombiner* combiner) OVERRIDE;

 private:
  // Our combiner.
  SuggestionsCombiner* combiner_;

  // Keep the results fetched from the discovery service here.
  std::deque<base::DictionaryValue*> items_;

  // Fetcher for the recommended pages.
  scoped_ptr<net::URLFetcher> recommended_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSourceDiscovery);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_DISCOVERY_H_
