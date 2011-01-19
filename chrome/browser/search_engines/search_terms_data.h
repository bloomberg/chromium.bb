// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"

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

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsData);
};

// Implementation of SearchTermsData that is only usable on the UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  UIThreadSearchTermsData();

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const;
  virtual std::string GetApplicationLocale() const;
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  virtual string16 GetRlzParameterValue() const;
#endif

  // Used by tests to set the value for the Google base url. This takes
  // ownership of the given std::string.
  static void SetGoogleBaseURL(std::string* google_base_url);

 private:
  static std::string* google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
