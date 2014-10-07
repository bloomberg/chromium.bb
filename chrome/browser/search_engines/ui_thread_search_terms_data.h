// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/search_engines/search_terms_data.h"

class Profile;

// Implementation of SearchTermsData that is only usable on the UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  // If |profile_| is NULL, the Google base URL accessors will return default
  // values, and NTPIsThemedParam() will return an empty string.
  explicit UIThreadSearchTermsData(Profile* profile);

  virtual std::string GoogleBaseURLValue() const override;
  virtual std::string GetApplicationLocale() const override;
  virtual base::string16 GetRlzParameterValue(bool from_app_list) const
      override;
  virtual std::string GetSearchClient() const override;
  virtual std::string GetSuggestClient() const override;
  virtual std::string GetSuggestRequestIdentifier() const override;
  virtual bool EnableAnswersInSuggest() const override;
  virtual bool IsShowingSearchTermsOnSearchResultsPages() const override;
  virtual std::string InstantExtendedEnabledParam(
      bool for_search) const override;
  virtual std::string ForceInstantResultsParam(
      bool for_prerender) const override;
  virtual int OmniboxStartMargin() const override;
  virtual std::string NTPIsThemedParam() const override;
  virtual std::string GoogleImageSearchSource() const override;

  // Used by tests to override the value for the Google base URL.  Passing the
  // empty string cancels this override.
  static void SetGoogleBaseURL(const std::string& base_url);

 private:
  static std::string* google_base_url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_
