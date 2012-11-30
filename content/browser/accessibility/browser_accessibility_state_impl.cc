// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/sys_color_change_listener.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

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

  // UpdateHistogram only takes a couple of milliseconds, but run it on
  // the FILE thread to guarantee there's no jank.
  // And we need to AddRef() the leaky singleton so that Bind doesn't
  // delete it prematurely.
  AddRef();
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BrowserAccessibilityStateImpl::UpdateHistogram, this),
      base::TimeDelta::FromSeconds(kAccessibilityHistogramDelaySecs));
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

void BrowserAccessibilityStateImpl::AddHistogramCallback(
    base::Closure callback) {
  histogram_callbacks_.push_back(callback);
}

void BrowserAccessibilityStateImpl::UpdateHistogram() {
  UpdatePlatformSpecificHistograms();

  for (size_t i = 0; i < histogram_callbacks_.size(); ++i)
    histogram_callbacks_[i].Run();

  UMA_HISTOGRAM_BOOLEAN("Accessibility.State", IsAccessibleBrowser());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.InvertedColors",
                        gfx::IsInvertedColorScheme());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kForceRendererAccessibility));
}

#if !defined(OS_WIN)
void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
}
#endif

AccessibilityMode BrowserAccessibilityStateImpl::GetAccessibilityMode() {
  return accessibility_mode_;
}

void BrowserAccessibilityStateImpl::SetAccessibilityMode(
    AccessibilityMode mode) {
  accessibility_mode_ = mode;
}

}  // namespace content
