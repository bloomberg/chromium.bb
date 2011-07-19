// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_

class CommandLine;

namespace prerender {

// Parse the --prerender= command line switch, which controls both prerendering
// and prefetching.  If the switch is unset, or is set to "auto", then the user
// is assigned to a field trial.
void ConfigurePrefetchAndPrerender(const CommandLine& command_line);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
