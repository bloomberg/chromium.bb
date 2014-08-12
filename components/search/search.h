// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_H_
#define COMPONENTS_SEARCH_SEARCH_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace chrome {

// Returns whether the Instant Extended API is enabled.
bool IsInstantExtendedAPIEnabled();

// Returns the value to pass to the &espv CGI parameter when loading the
// embedded search page from the user's default search provider. Returns 0 if
// the Instant Extended API is not enabled.
uint64 EmbeddedSearchPageVersion();

// Type for a collection of experiment configuration parameters.
typedef std::vector<std::pair<std::string, std::string> > FieldTrialFlags;

// Finds the active field trial group name and parses out the configuration
// flags. On success, |flags| will be filled with the field trial flags. |flags|
// must not be NULL. Returns true iff the active field trial is successfully
// parsed and not disabled.
// Note that |flags| may be successfully populated in some cases when false is
// returned - in these cases it should not be used.
// Exposed for testing only.
bool GetFieldTrialInfo(FieldTrialFlags* flags);

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

// Returns true if 'hide_verbatim' flag is enabled in field trials
// to hide the top match in the native suggestions dropdown if it is a verbatim
// match.  See comments on ShouldHideTopMatch in autocomplete_result.h.
bool ShouldHideTopVerbatimMatch();

}  // namespace chrome

#endif  // COMPONENTS_SEARCH_SEARCH_H_
