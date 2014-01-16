// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_COMBINER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_COMBINER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

class GURL;
class SuggestionsHandler;
class SuggestionsSource;
class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

// Combines many different sources of suggestions and generates data from it.
class SuggestionsCombiner {
 public:
  // Interface to be implemented by classes that will be notified of events from
  // the SuggestionsCombiner.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Method that is called when new suggestions are ready from the
    // SuggestionsCombiner.
    virtual void OnSuggestionsReady() = 0;
  };

  virtual ~SuggestionsCombiner();

  explicit SuggestionsCombiner(SuggestionsCombiner::Delegate* delegate,
                               Profile* profile);

  // Add a new source. The SuggestionsCombiner takes ownership of |source|.
  void AddSource(SuggestionsSource* source);

  // Enables or disables debug mode. If debug mode is enabled, the sources are
  // expected to provide additional data, which could be displayed, for example,
  // in the chrome://suggestions-internals/ page.
  void EnableDebug(bool enable);

  // Fetch a new set of items from the various suggestion sources.
  void FetchItems(Profile* profile);

  base::ListValue* GetPageValues();

  // Called by a source when its items are ready. Make sure suggestion sources
  // call this method exactly once for each call to
  // SuggestionsSource::FetchItems.
  void OnItemsReady();

  void SetSuggestionsCount(size_t suggestions_count);

 private:
  friend class SuggestionsCombinerTest;

  // Fill the page values from the suggestion sources so they can be sent to
  // the JavaScript side. This should only be called when all the suggestion
  // sources have items ready.
  void FillPageValues();

  // Add extra information to page values that should be common across all
  // suggestion sources.
  void AddExtendedInformation(base::DictionaryValue* page_value);

  // Checks if a URL is already open for the current profile. URLs open in an
  // incognito window are not reported.
  bool IsUrlAlreadyOpen(const GURL& url);

  typedef ScopedVector<SuggestionsSource> SuggestionsSources;

  // List of all the suggestions sources that will be combined to generate a
  // single list of suggestions.
  SuggestionsSources sources_;

  // Counter tracking the number of sources that are currently asynchronously
  // fetching their data.
  int sources_fetching_count_;

  // The delegate to notify once items are ready.
  SuggestionsCombiner::Delegate* delegate_;

  // Number of suggestions to generate. Used to distribute the suggestions
  // between the various sources.
  size_t suggestions_count_;

  // Informations to send to the javascript side.
  scoped_ptr<base::ListValue> page_values_;

  // Whether debug mode is enabled or not (debug mode provides more data in the
  // results).
  bool debug_enabled_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsCombiner);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_COMBINER_H_
