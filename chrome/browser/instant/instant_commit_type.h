// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
#pragma once

// Enum describing the ways instant can be committed.
enum InstantCommitType {
  // The commit is the result of the user pressing enter or tab.
  INSTANT_COMMIT_PRESSED_ENTER,

  // The commit is the result of focus being lost. This typically corresponds
  // to a mouse click event.
  INSTANT_COMMIT_FOCUS_LOST,

  // Used internally by InstantController.
  INSTANT_COMMIT_DESTROY
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_COMMIT_TYPE_H_
