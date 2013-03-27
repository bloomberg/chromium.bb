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
class PrefRegistrySyncable;
class Profile;
class TemplateURL;
class TemplateURLRef;

namespace content {
class NavigationEntry;
class WebContents;
}

namespace chrome {

// The key used to store search terms data in the NavigationEntry to be later
// displayed in the omnibox. With the context of the user's exact query,
// InstantController sets the correct search terms to be displayed.
extern const char kInstantExtendedSearchTermsKey[];

// Use this value for "start margin" to prevent the "es_sm" parameter from
// being used.
extern const int kDisableStartMargin;

// Returns whether the Instant Extended API is enabled.
bool IsInstantExtendedAPIEnabled();

// Returns the value to pass to the &espv CGI parameter when loading the
// embedded search page from the user's default search provider. Will be
// 0 if the Instant Extended API is not enabled.
uint64 EmbeddedSearchPageVersion();

// Returns whether query extraction is enabled.
bool IsQueryExtractionEnabled();

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
void RegisterInstantUserPrefs(PrefRegistrySyncable* registry);

// Returns prefs::kInstantExtendedEnabled in extended mode;
// prefs::kInstantEnabled otherwise.
const char* GetInstantPrefName();

// Returns whether the Instant pref (as per GetInstantPrefName()) is enabled.
bool IsInstantPrefEnabled(Profile* profile);

// Sets the default value of prefs::kInstantExtendedEnabled, based on field
// trials and the current value of prefs::kInstantEnabled.
void SetInstantExtendedPrefDefault(Profile* profile);

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

// Instant (loading a remote server page and talking to it using the searchbox
// API) is considered enabled if there's a valid Instant URL that can be used,
// so this simply returns whether GetInstantURL() is a valid URL.
// NOTE: This method expands the default search engine's instant_ur templatel,
// so it shouldn't be called from SearchTermsData or other such code that would
// lead to an infinite recursion.
bool IsInstantEnabled(Profile* profile);

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

}  // namespace chrome

#endif  // CHROME_BROWSER_SEARCH_SEARCH_H_
