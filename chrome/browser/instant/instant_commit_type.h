// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
#pragma once

// Reason why the Instant preview is committed (merged into a tab).
enum InstantCommitType {
  // The commit is due to the user pressing enter or tab from the omnibox.
  INSTANT_COMMIT_PRESSED_ENTER,

  // The commit is due to the omnibox losing focus, usually due to the user
  // clicking on the preview.
  INSTANT_COMMIT_FOCUS_LOST,

  // Used internally by InstantController.
  INSTANT_COMMIT_DESTROY
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
