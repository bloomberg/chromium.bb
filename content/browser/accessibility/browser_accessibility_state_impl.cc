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

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

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
      accessibility_mode_(AccessibilityModeOff) {
#if defined(OS_WIN)
  // On Windows 8, always enable accessibility for editable text controls
  // so we can show the virtual keyboard when one is enabled.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    accessibility_mode_ = AccessibilityModeEditableTextOnly;
  }
#endif  // defined(OS_WIN)

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    accessibility_mode_ = AccessibilityModeComplete;
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
  SetAccessibilityMode(AccessibilityModeComplete);
}

void BrowserAccessibilityStateImpl::OnAccessibilityEnabledManually() {
  // We may want to do something different with this later.
  SetAccessibilityMode(AccessibilityModeComplete);
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return (accessibility_mode_ == AccessibilityModeComplete);
}

void BrowserAccessibilityStateImpl::UpdateHistogram() {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.State",
                            IsAccessibleBrowser() ? 1 : 0,
                            2);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.InvertedColors",
                            gfx::IsInvertedColorScheme() ? 1 : 0,
                            2);
}

AccessibilityMode BrowserAccessibilityStateImpl::GetAccessibilityMode() {
  return accessibility_mode_;
}

void BrowserAccessibilityStateImpl::SetAccessibilityMode(
    AccessibilityMode mode) {
  accessibility_mode_ = mode;
}
