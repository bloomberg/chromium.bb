// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  // Returns the value to use for replacements of type GOOGLE_BASE_URL.  This
  // implementation simply returns the default value.
  virtual std::string GoogleBaseURLValue() const;

  // Returns the value for the GOOGLE_BASE_SUGGEST_URL term.  This
  // implementation simply returns the default value.
  std::string GoogleBaseSuggestURLValue() const;

  // Returns the locale used by the application.  This implementation returns
  // "en" and thus should be overridden where the result is actually meaningful.
  virtual std::string GetApplicationLocale() const;

#if defined(ENABLE_RLZ)
  // Returns the value for the Chrome Omnibox rlz.  This implementation returns
  // the empty string.
  virtual string16 GetRlzParameterValue() const;
#endif

  // Returns a string indicating whether Instant (in the visible-preview mode)
  // is enabled, suitable for adding as a query string param to the homepage
  // (instant_url) request. Returns an empty string if Instant is disabled,
  // or if it's only active in a hidden field trial mode. Determining this
  // requires accessing the Profile, so this can only ever be non-empty for
  // UIThreadSearchTermsData.
  virtual std::string InstantEnabledParam() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsData);
};

// Implementation of SearchTermsData that is only usable on the UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  UIThreadSearchTermsData();

  // Callers who need an accurate answer from InstantEnabledParam() must set the
  // profile here before calling them.
  void set_profile(Profile* profile) { profile_ = profile; }

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const OVERRIDE;
  virtual std::string GetApplicationLocale() const OVERRIDE;
#if defined(ENABLE_RLZ)
  virtual string16 GetRlzParameterValue() const OVERRIDE;
#endif

  // This returns the empty string unless set_profile() has been called with a
  // non-NULL Profile.
  virtual std::string InstantEnabledParam() const OVERRIDE;

  // Used by tests to override the value for the Google base URL.  Passing the
  // empty string cancels this override.
  static void SetGoogleBaseURL(const std::string& base_url);

 private:
  static std::string* google_base_url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
