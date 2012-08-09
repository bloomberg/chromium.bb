// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INSTANT_TYPES_H_
#define CHROME_COMMON_INSTANT_TYPES_H_

// Ways that the Instant suggested text is autocompleted into the omnibox.
enum InstantCompleteBehavior {
  // Autocomplete the suggestion immediately.
  INSTANT_COMPLETE_NOW,

  // Autocomplete the suggestion after a delay.
  INSTANT_COMPLETE_DELAYED,

  // Do not autocomplete the suggestion. The suggestion may still be displayed
  // in the omnibox, but not made a part of the omnibox text by default (e.g.,
  // by displaying the suggestion as non-highlighted, non-selected gray text).
  INSTANT_COMPLETE_NEVER,
};

#endif  // CHROME_COMMON_INSTANT_TYPES_H_
