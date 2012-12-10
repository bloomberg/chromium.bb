// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_

// Reason why the Instant preview is committed (merged into a tab).
enum InstantCommitType {
  // The commit is due to the user pressing Enter from the omnibox.
  INSTANT_COMMIT_PRESSED_ENTER,

  // The commit is due to the user pressing Alt-Enter from the omnibox (which
  // causes the preview to be committed on to a new tab).
  INSTANT_COMMIT_PRESSED_ALT_ENTER,

  // The commit is due to the omnibox losing focus, usually due to the user
  // clicking on the preview.
  INSTANT_COMMIT_FOCUS_LOST,

  // The commit is due to the instant page navigating.
  INSTANT_COMMIT_NAVIGATED,
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
