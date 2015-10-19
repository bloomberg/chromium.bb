// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ALWAYS_ON_TOP_WINDOW_KILLER_WIN_H_
#define CHROME_TEST_BASE_ALWAYS_ON_TOP_WINDOW_KILLER_WIN_H_

enum class RunType {
  // Indicates cleanup is happening before the test run.
  BEFORE_TEST,

  // Indicates cleanup is happening after the test run.
  AFTER_TEST,
};

// Logs if there are any always on top windows, and if one is a system dialog
// closes it.
void KillAlwaysOnTopWindows(RunType run_type);

#endif  // CHROME_TEST_BASE_ALWAYS_ON_TOP_WINDOW_KILLER_WIN_H_
