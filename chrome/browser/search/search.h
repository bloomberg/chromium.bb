// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_H_
#define CHROME_BROWSER_SEARCH_SEARCH_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

class GURL;
class Profile;
class TemplateURL;
class TemplateURLRef;

namespace content {
class NavigationEntry;
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome {

enum OptInState {
  // The user has not manually opted in/out of InstantExtended,
  // either local or regular. The in/out for local or not may
  // occur concurrently, but only once for each (local or not).
  INSTANT_EXTENDED_NOT_SET,
  // The user has opted-in to InstantExtended.
  INSTANT_EXTENDED_OPT_IN,
  // The user has opted-out of InstantExtended.
  INSTANT_EXTENDED_OPT_OUT,
  // The user has opted-in to Local InstantExtended.
  INSTANT_EXTENDED_OPT_IN_LOCAL,
  // The user has opted-out of Local InstantExtended.
  INSTANT_EXTENDED_OPT_OUT_LOCAL,
  // The user has opted-out of both Local and regular InstantExtended.
  INSTANT_EXTENDED_OPT_OUT_BOTH,
  INSTANT_EXTENDED_OPT_IN_STATE_ENUM_COUNT,
};

// Use this value for "start margin" to prevent the "es_sm" parameter from
// being used.
extern const int kDisableStartMargin;

// Returns whether the Instant Extended API is enabled.
bool IsInstantExtendedAPIEnabled();

// Returns the value to pass to the &espv CGI parameter when loading the
// embedded search page from the user's default search provider. Will be
// 0 if the Instant Extended API is not enabled, or if the local-only Instant
// Extended API is enabled.
uint64 EmbeddedSearchPageVersion();

// Returns whether query extraction is enabled.
bool IsQueryExtractionEnabled();

// Returns whether the local-only version of Instant Extended API is enabled.
bool IsLocalOnlyInstantExtendedAPIEnabled();

// Returns the search terms attached to a specific NavigationEntry, or empty
// string otherwise. Does not consider IsQueryExtractionEnabled(), so most
// callers should use GetSearchTerms() below instead.
string16 GetSearchTermsFromNavigationEntry(
    const content::NavigationEntry* entry);

// Returns search terms if this WebContents is a search results page. It looks
// in the visible NavigationEntry first, to see if search terms have already
// been extracted. Failing that, it tries to extract search terms from the URL.
// Returns a blank string if search terms were not found, or if search terms
// extraction is disabled for this WebContents or profile.
string16 GetSearchTerms(const content::WebContents* contents);

// Returns true if |url| should be rendered in the Instant renderer process.
bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile);

// Returns true if the visible entry of |contents| is a New Tab Page rendered
// by Instant. A page that matches the search or Instant URL of the default
// search provider but does not have any search terms is considered an Instant
// New Tab Page.
bool IsInstantNTP(const content::WebContents* contents);

// Same as IsInstantNTP but uses |nav_entry| to determine the URL for the page
// instead of using the visible entry.
bool NavEntryIsInstantNTP(const content::WebContents* contents,
                          const content::NavigationEntry* nav_entry);

// Registers Instant-related user preferences. Called at startup.
void RegisterInstantUserPrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns prefs::kInstantExtendedEnabled in extended mode;
// prefs::kInstantEnabled otherwise.
const char* GetInstantPrefName();

// Sets the default value of prefs::kInstantExtendedEnabled, based on field
// trials and the current value of prefs::kInstantEnabled.
void SetInstantExtendedPrefDefault(Profile* profile);

// Returns whether the Instant checkbox in chrome://settings/ should be enabled
// (i.e., toggleable). This returns true iff prefs::kSearchSuggestEnabled is
// true and the default search engine has a valid Instant URL in its template.
bool IsInstantCheckboxEnabled(Profile* profile);

// Returns whether the Instant checkbox in chrome://settings/ should be checked
// (i.e., with a tick mark). This returns true iff IsInstantCheckboxEnabled()
// and the pref indicated by GetInstantPrefName() is set to true.
bool IsInstantCheckboxChecked(Profile* profile);

// Returns the Instant URL of the default search engine. Returns an empty GURL
// if the engine doesn't have an Instant URL, or if it shouldn't be used (say
// because it doesn't satisfy the requirements for extended mode or if Instant
// is disabled through preferences). Callers must check that the returned URL is
// valid before using it. The value of |start_margin| is used for the "es_sm"
// parameter in the URL.
// NOTE: This method expands the default search engine's instant_url template,
// so it shouldn't be called from SearchTermsData or other such code that would
// lead to an infinite recursion.
GURL GetInstantURL(Profile* profile, int start_margin);

// Returns the Local Instant URL of the default search engine.  In particular,
// a Google search provider will include a special query parameter, indicating
// to the JS that Google-specific New Tab Page elements should be rendered.
GURL GetLocalInstantURL(Profile* profile);

// Instant (loading a remote server page and talking to it using the searchbox
// API) is considered enabled if there's a valid Instant URL that can be used,
// so this simply returns whether GetInstantURL() is a valid URL.
// NOTE: This method expands the default search engine's instant_url template,
// so it shouldn't be called from SearchTermsData or other such code that would
// lead to an infinite recursion.
bool IsInstantEnabled(Profile* profile);

// Returns true if 'use_remote_ntp_on_startup' flag is enabled in field trials
// to always show the remote NTP on browser startup.
bool ShouldPreferRemoteNTPOnStartup();

// Returns true if |my_url| matches |other_url|.
bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url);

// Transforms the input |url| into its "privileged URL". The returned URL
// facilitates grouping process-per-site. The |url| is transformed, for
// example, from
//
//   https://www.google.com/search?espv=1&q=tractors
//
// to the privileged URL
//
//   chrome-search://www.google.com/search?espv=1&q=tractors
//
// Notice the scheme change.
//
// If the input is already a privileged URL then that same URL is returned.
GURL GetPrivilegedURLForInstant(const GURL& url, Profile* profile);

// Returns true if the input |url| is a privileged Instant URL.
bool IsPrivilegedURLForInstant(const GURL& url);

// Returns the staleness timeout (in seconds) that should be used to refresh the
// InstantLoader.
int GetInstantLoaderStalenessTimeoutSec();

// -----------------------------------------------------
// The following APIs are exposed for use in tests only.
// -----------------------------------------------------

// Forces the Instant Extended API to be enabled for tests.
void EnableInstantExtendedAPIForTesting();

// Type for a collection of experiment configuration parameters.
typedef std::vector<std::pair<std::string, std::string> > FieldTrialFlags;

// Given a field trial group name, parses out the group number and configuration
// flags. On success, |flags| will be filled with the field trial flags. |flags|
// must not be NULL. If not NULL, |group_number| will receive the experiment
// group number.
// Returns true iff field trial info was successfully parsed out of
// |group_name|.
// Exposed for testing only.
bool GetFieldTrialInfo(const std::string& group_name,
                       FieldTrialFlags* flags,
                       uint64* group_number);

// Given a FieldTrialFlags object, returns the string value of the provided
// flag.
// Exposed for testing only.
std::string GetStringValueForFlagWithDefault(const std::string& flag,
                                             const std::string& default_value,
                                             const FieldTrialFlags& flags);

// Given a FieldTrialFlags object, returns the uint64 value of the provided
// flag.
// Exposed for testing only.
uint64 GetUInt64ValueForFlagWithDefault(const std::string& flag,
                                        uint64 default_value,
                                        const FieldTrialFlags& flags);

// Given a FieldTrialFlags object, returns the bool value of the provided flag.
// Exposed for testing only.
bool GetBoolValueForFlagWithDefault(const std::string& flag,
                                    bool default_value,
                                    const FieldTrialFlags& flags);

// Coerces the commandline Instant URL to look like a template URL, so that we
// can extract search terms from it. Exposed for testing only.
GURL CoerceCommandLineURLToTemplateURL(const GURL& instant_url,
                                       const TemplateURLRef& ref,
                                       int start_margin);

// Returns whether the default search provider has a valid Instant URL in its
// template. Exposed for testing only.
bool DefaultSearchProviderSupportsInstant(Profile* profile);

// Let tests reset the gate that prevents metrics from being sent more than
// once.
void ResetInstantExtendedOptInStateGateForTest();

}  // namespace chrome

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
