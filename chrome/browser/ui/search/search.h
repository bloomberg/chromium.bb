// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_H_

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

namespace chrome {
namespace search {

// The key used to store search terms data in the NavigationEntry to be later
// displayed in the omnibox. With the context of the user's exact query,
// InstantController sets the correct search terms to be displayed.
extern const char kInstantExtendedSearchTermsKey[];

// The URL for the local omnibox popup (rendered in a WebContents).
extern const char kLocalOmniboxPopupURL[];

// The default value we should assign to the instant_extended.enabled pref.
// As with other prefs, the default is used only when the user hasn't toggled
// the pref explicitly.
enum InstantExtendedDefault {
  INSTANT_DEFAULT_ON,    // Default the pref to be enabled.
  INSTANT_USE_EXISTING,  // Use the current value of the instant.enabled pref.
  INSTANT_DEFAULT_OFF,   // Default the pref to be disabled.
};

// Returns an enum value indicating which mode to set the new
// instant_extended.enabled pref to by default.
InstantExtendedDefault GetInstantExtendedDefaultSetting();

// Returns whether the Instant Extended API is enabled in this profile.
bool IsInstantExtendedAPIEnabled(const Profile* profile);

// Returns the value to pass to the &espv CGI parameter when loading the
// embedded search page from the user's default search provider. Will be
// 0 if the Instant Extended API is not enabled.
uint64 EmbeddedSearchPageVersion(const Profile* profile);

// Returns whether query extraction is enabled.
bool IsQueryExtractionEnabled(const Profile* profile);

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

// -----------------------------------------------------
// The following APIs are exposed for use in tests only.
// -----------------------------------------------------

// Forces the Instant Extended API to be enabled for tests.
void EnableInstantExtendedAPIForTesting();

// Forces query extraction to be enabled for tests.
void EnableQueryExtractionForTesting();

// Actually implements the logic for ShouldAssignURLToInstantRenderer().
// Exposed for testing only.
bool ShouldAssignURLToInstantRendererImpl(const GURL& url,
                                          bool extended_api_enabled,
                                          TemplateURL* template_url);

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
// can extract search terms from it.
GURL CoerceCommandLineURLToTemplateURL(const GURL& instant_url,
                                       const TemplateURLRef& ref);

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_H_
