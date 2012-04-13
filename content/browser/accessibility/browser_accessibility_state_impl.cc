// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/timer.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/sys_color_change_listener.h"

// Update the accessibility histogram 45 seconds after initialization.
static const int kAccessibilityHistogramDelaySecs = 45;

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  return Singleton<BrowserAccessibilityStateImpl,
                   LeakySingletonTraits<BrowserAccessibilityStateImpl> >::get();
}

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(),
      accessibility_enabled_(false) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    OnAccessibilityEnabledManually();
  }
  update_histogram_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kAccessibilityHistogramDelaySecs),
      this,
      &BrowserAccessibilityStateImpl::UpdateHistogram);
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }
  accessibility_enabled_ = true;
}

void BrowserAccessibilityStateImpl::OnAccessibilityEnabledManually() {
  // We may want to do something different with this later.
  accessibility_enabled_ = true;
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return accessibility_enabled_;
}

void BrowserAccessibilityStateImpl::UpdateHistogram() {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.State",
                            accessibility_enabled_ ? 1 : 0,
                            2);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.InvertedColors",
                            gfx::IsInvertedColorScheme() ? 1 : 0,
                            2);
}
