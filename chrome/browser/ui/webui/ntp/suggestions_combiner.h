// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_COMBINER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_COMBINER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class SuggestionsHandler;
class SuggestionsSource;
class Profile;

namespace base {
  class ListValue;
}

// Combines many different sources of suggestions and generates data from it.
class SuggestionsCombiner {
 public:
  explicit SuggestionsCombiner(SuggestionsHandler* suggestions_handler);
  virtual ~SuggestionsCombiner();

  // Add a new source.
  void AddSource(SuggestionsSource* source);

  // Fetch a new set of items from the various suggestion sources.
  void FetchItems(Profile* profile);

  base::ListValue* GetPagesValue();

  // Called by a source when its items are ready. Make sure suggestion sources
  // call this method exactly once for each call to
  // SuggestionsSource::FetchItems.
  void OnItemsReady();

 private:
  // Fill the pages value from the suggestion sources so they can be sent to
  // the javascript side. This should only be called when all the suggestion
  // sources have items ready.
  void FillPagesValue();

  typedef std::vector<SuggestionsSource*> SuggestionsSources;

  // List of all the suggestions sources that will be combined to generate a
  // single list of suggestions.
  SuggestionsSources sources_;

  // Counter tracking the number of sources that are currently asynchronously
  // fetching their data.
  int sources_fetching_count_;

  // The suggestions handler to notify once items are ready.
  SuggestionsHandler* suggestions_handler_;

  // Number of suggestions to generate. Used to distribute the suggestions
  // between the various sources.
  size_t suggestions_count_;

  // Informations to send to the javascript side.
  scoped_ptr<base::ListValue> pages_value_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsCombiner);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_COMBINER_H_
