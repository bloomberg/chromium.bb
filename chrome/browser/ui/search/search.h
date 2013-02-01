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

namespace content {
class NavigationEntry;
}

namespace chrome {
namespace search {

// The key used to store search terms data in the NavigationEntry to be later
// displayed in the Omnibox. With the context of the user's exact query,
// InstantController sets the correct search terms to be displayed.
extern const char kInstantExtendedSearchTermsKey[];

enum InstantExtendedDefault {
  INSTANT_FORCE_ON,      // Force the setting on if no other setting exists.
  INSTANT_USE_EXISTING,  // Use same the value of the old instant.enabled pref.
  INSTANT_FORCE_OFF,     // Force the setting off if no other setting exists.
};

// Returns an enum value indicating which mode to set the new
// instant_extended.enabled pref to by default.
InstantExtendedDefault GetInstantExtendedDefaultSetting();

// Returns whether the Instant extended API is enabled for the given |profile|.
// |profile| may not be NULL.
bool IsInstantExtendedAPIEnabled(Profile* profile);

// Returns the value to pass to the &espv cgi parameter when loading the
// embedded search page from the user's default search provider.  Will be
// 0 if the Instant Extended API is not enabled.
uint64 EmbeddedSearchPageVersion(Profile* profile);

// Force the instant extended API to be enabled for tests.
void EnableInstantExtendedAPIForTesting();

// Returns whether query extraction is enabled.  If
// |IsInstantExtendedAPIEnabled()| and the profile is not off the record, then
// this method will also return true.
bool IsQueryExtractionEnabled(Profile* profile);

// Force query extraction to be enabled for tests.
void EnableQueryExtractionForTesting();

// Return the search terms attached to a specific NavigationEntry, or empty
// string otherwise.
string16 GetSearchTermsFromNavigationEntry(
    const content::NavigationEntry* entry);

// Returns true if |url| has the same scheme, host, port and path as the
// Instant URL set via --instant-url.
bool IsForcedInstantURL(const GURL& url);

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
std::string GetStringValueForFlagWithDefault(
    const std::string& flag,
    const std::string& default_value,
    FieldTrialFlags& flags);

// Given a FieldTrialFlags object, returns the uint64 value of the provided
// flag.
// Exposed for testing only.
uint64 GetUInt64ValueForFlagWithDefault(
    const std::string& flag, uint64 default_value, FieldTrialFlags& flags);

// Given a FieldTrialFlags object, returns the bool value of the provided flag.
// Exposed for testing only.
bool GetBoolValueForFlagWithDefault(
    const std::string& flag, bool default_value, FieldTrialFlags& flags);

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_H_
