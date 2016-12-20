// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/color_utils.h"

namespace content {

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum ModeFlagHistogramValue {
  UMA_AX_MODE_FLAG_NATIVE_APIS = 0,
  UMA_AX_MODE_FLAG_WEB_CONTENTS = 1,
  UMA_AX_MODE_FLAG_INLINE_TEXT_BOXES = 2,
  UMA_AX_MODE_FLAG_SCREEN_READER = 3,
  UMA_AX_MODE_FLAG_HTML = 4,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_AX_MODE_FLAG_MAX
};

// Record a histograms for an accessibility mode when it's enabled.
void RecordNewAccessibilityModeFlags(ModeFlagHistogramValue mode_flag) {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.ModeFlag",
                            mode_flag,
                            UMA_AX_MODE_FLAG_MAX);
}

// Update the accessibility histogram 45 seconds after initialization.
static const int ACCESSIBILITY_HISTOGRAM_DELAY_SECS = 45;

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  return base::Singleton<
      BrowserAccessibilityStateImpl,
      base::LeakySingletonTraits<BrowserAccessibilityStateImpl>>::get();
}

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(),
      accessibility_mode_(AccessibilityModeOff),
      disable_hot_tracking_(false) {
  ResetAccessibilityModeValue();
#if defined(OS_WIN)
  // On Windows, UpdateHistograms calls some system functions with unknown
  // runtime, so call it on the file thread to ensure there's no jank.
  // Everything in that method must be safe to call on another thread.
  BrowserThread::ID update_histogram_thread = BrowserThread::FILE;
#else
  // On all other platforms, UpdateHistograms should be called on the main
  // thread.
  BrowserThread::ID update_histogram_thread = BrowserThread::UI;
#endif

  // We need to AddRef() the leaky singleton so that Bind doesn't
  // delete it prematurely.
  AddRef();
  BrowserThread::PostDelayedTask(
      update_histogram_thread, FROM_HERE,
      base::Bind(&BrowserAccessibilityStateImpl::UpdateHistograms, this),
      base::TimeDelta::FromSeconds(ACCESSIBILITY_HISTOGRAM_DELAY_SECS));
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }
  EnableAccessibility();
}

void BrowserAccessibilityStateImpl::EnableAccessibility() {
  AddAccessibilityModeFlags(ACCESSIBILITY_MODE_COMPLETE);
}

void BrowserAccessibilityStateImpl::DisableAccessibility() {
  ResetAccessibilityMode();
}

void BrowserAccessibilityStateImpl::ResetAccessibilityModeValue() {
  accessibility_mode_ = AccessibilityModeOff;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    accessibility_mode_ = ACCESSIBILITY_MODE_COMPLETE;
  }
}

void BrowserAccessibilityStateImpl::ResetAccessibilityMode() {
  ResetAccessibilityModeValue();

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode());
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return ((accessibility_mode_ & ACCESSIBILITY_MODE_COMPLETE) ==
          ACCESSIBILITY_MODE_COMPLETE);
}

void BrowserAccessibilityStateImpl::AddHistogramCallback(
    base::Closure callback) {
  histogram_callbacks_.push_back(callback);
}

void BrowserAccessibilityStateImpl::UpdateHistogramsForTesting() {
  UpdateHistograms();
}

void BrowserAccessibilityStateImpl::UpdateHistograms() {
  UpdatePlatformSpecificHistograms();

  for (size_t i = 0; i < histogram_callbacks_.size(); ++i)
    histogram_callbacks_[i].Run();

  // TODO(dmazzoni): remove this in M59 since Accessibility.ModeFlag
  // supercedes it.  http://crbug.com/672205
  UMA_HISTOGRAM_BOOLEAN("Accessibility.State", IsAccessibleBrowser());

  UMA_HISTOGRAM_BOOLEAN("Accessibility.InvertedColors",
                        color_utils::IsInvertedColorScheme());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kForceRendererAccessibility));
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
}
#endif

void BrowserAccessibilityStateImpl::AddAccessibilityModeFlags(
    AccessibilityMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }

  AccessibilityMode previous_mode = accessibility_mode_;
  accessibility_mode_ |= mode;
  if (accessibility_mode_ == previous_mode)
    return;

  // Retrieve only newly added modes for the purposes of logging.
  AccessibilityMode new_mode_flags = accessibility_mode_ & (~previous_mode);
  if (new_mode_flags & ACCESSIBILITY_MODE_FLAG_NATIVE_APIS)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_FLAG_NATIVE_APIS);
  if (new_mode_flags & ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_FLAG_WEB_CONTENTS);
  if (new_mode_flags & ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_FLAG_INLINE_TEXT_BOXES);
  if (new_mode_flags & ACCESSIBILITY_MODE_FLAG_SCREEN_READER)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_FLAG_SCREEN_READER);
  if (new_mode_flags & ACCESSIBILITY_MODE_FLAG_HTML)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_FLAG_HTML);

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->AddAccessibilityMode(accessibility_mode_);
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityModeFlags(
    AccessibilityMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility) &&
      mode == ACCESSIBILITY_MODE_COMPLETE) {
    return;
  }

  accessibility_mode_ = accessibility_mode_ ^ (mode & accessibility_mode_);
  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode());
}

}  // namespace content
