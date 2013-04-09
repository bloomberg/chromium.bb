// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_OMNIBOX_FOCUS_STATE_H_
#define CHROME_COMMON_OMNIBOX_FOCUS_STATE_H_

// Omnibox focus state.
enum OmniboxFocusState {
  // Not focused.
  OMNIBOX_FOCUS_NONE,

  // Visibly focused.
  OMNIBOX_FOCUS_VISIBLE,

  // Invisibly focused, i.e. focused with a hidden caret.
  //
  // Omnibox will not look focused visibly but any user key strokes will go to
  // the omnibox.
  OMNIBOX_FOCUS_INVISIBLE,
};

#endif  // CHROME_COMMON_OMNIBOX_FOCUS_STATE_H_
