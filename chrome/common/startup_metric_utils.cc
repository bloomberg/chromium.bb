// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/startup_metric_utils.h"

#include "base/logging.h"
#include "base/time.h"

namespace {

// Mark as volatile to defensively make sure usage is thread-safe.
// Note that at the time of this writing, access is only on the UI thread.
static volatile bool g_non_browser_ui_displayed = false;

const base::Time* MainEntryPointTimeInternal() {
  static base::Time main_start_time = base::Time::Now();
  return &main_start_time;
}

static bool g_main_entry_time_was_recorded = false;
}  // namespace

namespace startup_metric_utils {

bool WasNonBrowserUIDisplayed() {
  return g_non_browser_ui_displayed;
}

void SetNonBrowserUIDisplayed() {
  g_non_browser_ui_displayed = true;
}

void RecordMainEntryPointTime() {
  DCHECK(!g_main_entry_time_was_recorded);
  g_main_entry_time_was_recorded = true;
  MainEntryPointTimeInternal();
}

const base::Time MainEntryStartTime() {
  DCHECK(g_main_entry_time_was_recorded);
  return *MainEntryPointTimeInternal();
}

}  // namespace startup_metric_utils
