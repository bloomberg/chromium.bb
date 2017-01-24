// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
#define CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

#include <string>

#include "base/trace_event/trace_event_argument.h"

namespace cc {

// Ensure this stays in sync with MainThreadScrollingReason in histograms.xml.
// When adding a new MainThreadScrollingReason, make sure the corresponding
// [MainThread/Compositor]CanSetScrollReasons function is also updated.
struct MainThreadScrollingReason {
  enum : uint32_t {
    // Non-transient scrolling reasons.
    kNotScrollingOnMain = 0,
    kHasBackgroundAttachmentFixedObjects = 1 << 0,
    kHasNonLayerViewportConstrainedObjects = 1 << 1,
    kThreadedScrollingDisabled = 1 << 2,
    kScrollbarScrolling = 1 << 3,
    kPageOverlay = 1 << 4,

    // This bit is set when any of the other main thread scrolling reasons cause
    // an input event to be handled on the main thread, and the main thread
    // blink::ScrollAnimator is in the middle of running a scroll offset
    // animation. Note that a scroll handled by the main thread can result in an
    // animation running on the main thread or on the compositor thread.
    kHandlingScrollFromMainThread = 1 << 13,
    kCustomScrollbarScrolling = 1 << 15,

    // Style-related scrolling on main reasons.
    // These *AndLCDText reasons are due to subpixel text rendering which can
    // only be applied by blending glyphs with the background at a specific
    // screen position; transparency and transforms break this.
    kHasOpacityAndLCDText = 1 << 16,
    kHasTransformAndLCDText = 1 << 17,
    kBackgroundNotOpaqueInRectAndLCDText = 1 << 18,
    kHasBorderRadius = 1 << 19,
    kHasClipRelatedProperty = 1 << 20,
    kHasBoxShadowFromNonRootLayer = 1 << 21,

    // Transient scrolling reasons. These are computed for each scroll begin.
    kNonFastScrollableRegion = 1 << 5,
    kFailedHitTest = 1 << 7,
    kNoScrollingLayer = 1 << 8,
    kNotScrollable = 1 << 9,
    kContinuingMainThreadScroll = 1 << 10,
    kNonInvertibleTransform = 1 << 11,
    kPageBasedScrolling = 1 << 12,

    // The maximum number of flags in this struct (excluding itself).
    // New flags should increment this number but it should never be decremented
    // because the values are used in UMA histograms. It should also be noted
    // that it excludes the kNotScrollingOnMain value.
    kMainThreadScrollingReasonCount = 22,
  };

  // Returns true if the given MainThreadScrollingReason can be set by the main
  // thread.
  static bool MainThreadCanSetScrollReasons(uint32_t reasons) {
    uint32_t reasons_set_by_main_thread =
        kNotScrollingOnMain | kHasBackgroundAttachmentFixedObjects |
        kHasNonLayerViewportConstrainedObjects | kThreadedScrollingDisabled |
        kScrollbarScrolling | kPageOverlay | kHandlingScrollFromMainThread |
        kCustomScrollbarScrolling | kHasOpacityAndLCDText |
        kHasTransformAndLCDText | kBackgroundNotOpaqueInRectAndLCDText |
        kHasBorderRadius | kHasClipRelatedProperty |
        kHasBoxShadowFromNonRootLayer;
    return (reasons & reasons_set_by_main_thread) == reasons;
  }

  // Returns true if the given MainThreadScrollingReason can be set by the
  // compositor.
  static bool CompositorCanSetScrollReasons(uint32_t reasons) {
    uint32_t reasons_set_by_compositor =
        kNonFastScrollableRegion | kFailedHitTest | kNoScrollingLayer |
        kNotScrollable | kContinuingMainThreadScroll | kNonInvertibleTransform |
        kPageBasedScrolling;
    return (reasons & reasons_set_by_compositor) == reasons;
  }

  static std::string mainThreadScrollingReasonsAsText(uint32_t reasons) {
    base::trace_event::TracedValue tracedValue;
    mainThreadScrollingReasonsAsTracedValue(reasons, &tracedValue);
    std::string result_in_array_foramt = tracedValue.ToString();
    // Remove '{main_thread_scrolling_reasons:[', ']}', and any '"' chars.
    std::string result =
        result_in_array_foramt.substr(34, result_in_array_foramt.length() - 36);
    result.erase(std::remove(result.begin(), result.end(), '\"'), result.end());
    return result;
  }

  static void mainThreadScrollingReasonsAsTracedValue(
      uint32_t reasons,
      base::trace_event::TracedValue* tracedValue) {
    tracedValue->BeginArray("main_thread_scrolling_reasons");
    if (reasons &
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects)
      tracedValue->AppendString("Has background-attachment:fixed");
    if (reasons &
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects)
      tracedValue->AppendString("Has non-layer viewport-constrained objects");
    if (reasons & MainThreadScrollingReason::kThreadedScrollingDisabled)
      tracedValue->AppendString("Threaded scrolling is disabled");
    if (reasons & MainThreadScrollingReason::kScrollbarScrolling)
      tracedValue->AppendString("Scrollbar scrolling");
    if (reasons & MainThreadScrollingReason::kPageOverlay)
      tracedValue->AppendString("Page overlay");
    if (reasons & MainThreadScrollingReason::kHandlingScrollFromMainThread)
      tracedValue->AppendString("Handling scroll from main thread");
    if (reasons & MainThreadScrollingReason::kCustomScrollbarScrolling)
      tracedValue->AppendString("Custom scrollbar scrolling");
    if (reasons & MainThreadScrollingReason::kHasOpacityAndLCDText)
      tracedValue->AppendString("Has opacity and LCD text");
    if (reasons & MainThreadScrollingReason::kHasTransformAndLCDText)
      tracedValue->AppendString("Has transform and LCD text");
    if (reasons &
        MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText) {
      tracedValue->AppendString(
          "Background is not opaque in rect and LCD text");
    }
    if (reasons & MainThreadScrollingReason::kHasBorderRadius)
      tracedValue->AppendString("Has border radius");
    if (reasons & MainThreadScrollingReason::kHasClipRelatedProperty)
      tracedValue->AppendString("Has clip related property");
    if (reasons & MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer)
      tracedValue->AppendString("Has box shadow from non-root layer");

    // Transient scrolling reasons.
    if (reasons & MainThreadScrollingReason::kNonFastScrollableRegion)
      tracedValue->AppendString("Non fast scrollable region");
    if (reasons & MainThreadScrollingReason::kFailedHitTest)
      tracedValue->AppendString("Failed hit test");
    if (reasons & MainThreadScrollingReason::kNoScrollingLayer)
      tracedValue->AppendString("No scrolling layer");
    if (reasons & MainThreadScrollingReason::kNotScrollable)
      tracedValue->AppendString("Not scrollable");
    if (reasons & MainThreadScrollingReason::kContinuingMainThreadScroll)
      tracedValue->AppendString("Continuing main thread scroll");
    if (reasons & MainThreadScrollingReason::kNonInvertibleTransform)
      tracedValue->AppendString("Non-invertible transform");
    if (reasons & MainThreadScrollingReason::kPageBasedScrolling)
      tracedValue->AppendString("Page-based scrolling");
    tracedValue->EndArray();
  }

  // For a given reason, return its index in enum
  static int getReasonIndex(uint32_t reason) {
    // Multiple reasons provided
    if (reason & (reason - 1))
      return -1;

    int index = -1;
    while (reason > 0) {
      reason = reason >> 1;
      ++index;
    }
    return index;
  }
};

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
