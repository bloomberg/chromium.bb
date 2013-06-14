// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_

#include <string>

class CommandLine;
class Profile;

namespace prerender {

// Parse the --prerender= command line switch, which controls both prerendering
// and prefetching.  If the switch is unset, or is set to "auto", then the user
// is assigned to a field trial.
void ConfigurePrefetchAndPrerender(const CommandLine& command_line);

// Returns true if the user has opted in or has been opted in to the
// prerendering from Omnibox experiment.
bool IsOmniboxEnabled(Profile* profile);

// Returns true iff the Prerender Local Predictor is enabled.
bool IsLocalPredictorEnabled();

// Returns true iff the LoggedIn Predictor is enabled.
bool IsLoggedInPredictorEnabled();

// Returns true iff the side-effect free whitelist is enabled.
bool IsSideEffectFreeWhitelistEnabled();

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
