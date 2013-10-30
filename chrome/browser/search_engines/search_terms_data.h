// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"

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

  // Returns the value for the Chrome Omnibox rlz.  This implementation returns
  // the empty string.
  virtual string16 GetRlzParameterValue() const;

  // The optional client parameter passed with Google search requests.  This
  // implementation returns the empty string.
  virtual std::string GetSearchClient() const;

  // The client parameter passed with Google suggest requests.  This
  // implementation returns the empty string.
  virtual std::string GetSuggestClient() const;

  // Returns a string that will cause the search results page to update
  // incrementally. Currently, Instant Extended passes a different param to
  // search results pages that also has this effect, so by default this function
  // returns the empty string when Instant Extended is enabled. However, when
  // doing instant search result prerendering, we still need to pass this param,
  // as Instant Extended does not cause incremental updates by default for the
  // prerender page. Callers should set |for_prerender| in this case to force
  // the returned string to be non-empty.
  virtual std::string ForceInstantResultsParam(bool for_prerender) const;

  // Returns a string indicating whether InstantExtended is enabled, suitable
  // for adding as a query string param to the homepage or search requests.
  // Returns an empty string otherwise.  Determining this requires accessing the
  // Profile, so this can only ever be non-empty for UIThreadSearchTermsData.
  virtual std::string InstantExtendedEnabledParam() const;

  // Returns a string indicating whether a non-default theme is active,
  // suitable for adding as a query string param to the homepage.  This only
  // applies if Instant Extended is enabled.  Returns an empty string otherwise.
  // Determining this requires accessing the Profile, so this can only ever be
  // non-empty for UIThreadSearchTermsData.
  virtual std::string NTPIsThemedParam() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsData);
};

// Implementation of SearchTermsData that is only usable on the UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  // If |profile_| is NULL, the Google base URL accessors will return default
  // values, and ForceInstantResultsParam(), InstantExtendedEnabledParam(), and
  // NTPIsThemedParam(), will return the empty string.
  explicit UIThreadSearchTermsData(Profile* profile);

  virtual std::string GoogleBaseURLValue() const OVERRIDE;
  virtual std::string GetApplicationLocale() const OVERRIDE;
  virtual string16 GetRlzParameterValue() const OVERRIDE;
  virtual std::string GetSearchClient() const OVERRIDE;
  virtual std::string GetSuggestClient() const OVERRIDE;
  virtual std::string ForceInstantResultsParam(
      bool for_prerender) const OVERRIDE;
  virtual std::string InstantExtendedEnabledParam() const OVERRIDE;
  virtual std::string NTPIsThemedParam() const OVERRIDE;

  // Used by tests to override the value for the Google base URL.  Passing the
  // empty string cancels this override.
  static void SetGoogleBaseURL(const std::string& base_url);

 private:
  static std::string* google_base_url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
