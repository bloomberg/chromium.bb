// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"

class Profile;

// All data needed by TemplateURLRef::ReplaceSearchTerms which typically may
// only be accessed on the UI thread.
class SearchTermsData {
 public:
  SearchTermsData();
  virtual ~SearchTermsData();

  // Returns the value for the GOOGLE_BASE_SUGGEST_URL term.
  std::string GoogleBaseSuggestURLValue() const;

  // Returns the value to use for replacements of type GOOGLE_BASE_URL.
  virtual std::string GoogleBaseURLValue() const = 0;

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale() const = 0;

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // Returns the value for the Chrome Omnibox rlz.
  virtual string16 GetRlzParameterValue() const = 0;
#endif

  // Returns a string indicating the Instant field trial group, suitable for
  // adding as a query string param to suggest/search URLs, or an empty string
  // if the field trial is not active. Checking the field trial group requires
  // accessing the Profile, which means this can only ever be non-empty for
  // UIThreadSearchTermsData.
  virtual std::string InstantFieldTrialUrlParam() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsData);
};

// Implementation of SearchTermsData that is only usable on the UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  UIThreadSearchTermsData();

  // Callers who need an accurate answer from InstantFieldTrialUrlParam() must
  // set the profile here before calling that.
  void set_profile(Profile* profile) { profile_ = profile; }

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const OVERRIDE;
  virtual std::string GetApplicationLocale() const OVERRIDE;
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  virtual string16 GetRlzParameterValue() const OVERRIDE;
#endif

  // This returns the empty string unless set_profile() has been called with a
  // non-NULL Profile.
  virtual std::string InstantFieldTrialUrlParam() const OVERRIDE;

  // Used by tests to set the value for the Google base url. This takes
  // ownership of the given std::string.
  static void SetGoogleBaseURL(std::string* google_base_url);

 private:
  static std::string* google_base_url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
