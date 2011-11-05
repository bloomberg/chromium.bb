// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state.h"

#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/timer.h"

// Update the accessibility histogram 45 seconds after initialization.
static const int kAccessibilityHistogramDelaySecs = 45;

BrowserAccessibilityState::BrowserAccessibilityState()
    : accessibility_enabled_(false) {
  update_histogram_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kAccessibilityHistogramDelaySecs),
      this,
      &BrowserAccessibilityState::UpdateHistogram);
}

BrowserAccessibilityState::~BrowserAccessibilityState() {
}

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return Singleton<BrowserAccessibilityState>::get();
}

void BrowserAccessibilityState::OnScreenReaderDetected() {
  accessibility_enabled_ = true;
}

void BrowserAccessibilityState::OnAccessibilityEnabledManually() {
  // We may want to do something different with this later.
  accessibility_enabled_ = true;
}

bool BrowserAccessibilityState::IsAccessibleBrowser() {
  return accessibility_enabled_;
}

void BrowserAccessibilityState::UpdateHistogram() {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.State",
                            accessibility_enabled_ ? 1 : 0,
                            2);
}
