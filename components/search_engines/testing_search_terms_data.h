// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TESTING_SEARCH_TERMS_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TESTING_SEARCH_TERMS_DATA_H_

#include "components/search_engines/search_terms_data.h"

class TestingSearchTermsData : public SearchTermsData {
 public:
  explicit TestingSearchTermsData(const std::string& google_base_url);
  virtual ~TestingSearchTermsData();

  virtual std::string GoogleBaseURLValue() const OVERRIDE;
  virtual base::string16 GetRlzParameterValue(
      bool from_app_list) const OVERRIDE;
  virtual std::string GetSearchClient() const OVERRIDE;
  virtual std::string GoogleImageSearchSource() const OVERRIDE;
  virtual bool EnableAnswersInSuggest() const OVERRIDE;
  virtual bool IsShowingSearchTermsOnSearchResultsPages() const OVERRIDE;
  virtual int OmniboxStartMargin() const OVERRIDE;

  void set_google_base_url(const std::string& google_base_url) {
    google_base_url_ = google_base_url;
  }
  void set_search_client(const std::string& search_client) {
    search_client_ = search_client;
  }
  void set_enable_answers_in_suggest(bool enable_answers_in_suggest) {
    enable_answers_in_suggest_ = enable_answers_in_suggest;
  }
  void set_is_showing_search_terms_on_search_results_pages(bool value) {
    is_showing_search_terms_on_search_results_pages_ = value;
  }
  void set_omnibox_start_margin(int omnibox_start_margin) {
    omnibox_start_margin_ = omnibox_start_margin;
  }

 private:
  std::string google_base_url_;
  std::string search_client_;
  bool enable_answers_in_suggest_;
  bool is_showing_search_terms_on_search_results_pages_;
  int omnibox_start_margin_;

  DISALLOW_COPY_AND_ASSIGN(TestingSearchTermsData);
};

#endif  // COMPONENTS_SEARCH_ENGINES_TESTING_SEARCH_TERMS_DATA_H_
