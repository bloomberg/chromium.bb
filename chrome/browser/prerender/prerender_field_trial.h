// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_

#include <string>

class CommandLine;
class Profile;

namespace prerender {

enum OmniboxHeuristic {
  OMNIBOX_HEURISTIC_ORIGINAL,
  OMNIBOX_HEURISTIC_CONSERVATIVE,
  OMNIBOX_HEURISTIC_EXACT,
  OMNIBOX_HEURISTIC_EXACT_FULL,
  OMNIBOX_HEURISTIC_MAX
};

// Parse the --prerender= command line switch, which controls both prerendering
// and prefetching.  If the switch is unset, or is set to "auto", then the user
// is assigned to a field trial.
void ConfigurePrefetchAndPrerender(const CommandLine& command_line);

// Returns true if the user has opted in or has been opted in to the
// prerendering from Omnibox experiment.
bool IsOmniboxEnabled(Profile* profile);

// Returns the heuristic to use when determining if prerendering should be
// attempted from the Omnibox. Governed by a field trial.
OmniboxHeuristic GetOmniboxHeuristicToUse();

// Returns the suffix to use for histograms dependent on which Omnibox heuristic
// is active.
std::string GetOmniboxHistogramSuffix();

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
