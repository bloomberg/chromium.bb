// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_ORIGIN_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_ORIGIN_H_

namespace prerender {

// Origin indicates what caused the prerender.
// NOTE: New values need to be appended, since they are used in histograms.
enum Origin {
  ORIGIN_LINK_REL_PRERENDER = 0,
  // Obsolete: ORIGIN_OMNIBOX_ORIGINAL = 1,
  ORIGIN_GWS_PRERENDER = 2,
  // Obsolete: ORIGIN_OMNIBOX_CONSERVATIVE = 3,
  ORIGIN_OMNIBOX_EXACT = 4,
  ORIGIN_OMNIBOX_EXACT_FULL = 5,
  ORIGIN_MAX
};

// Return a human-readable name for |origin|.
const char* NameFromOrigin(Origin origin);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_ORIGIN_H_
