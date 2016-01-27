// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
#define CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_

namespace cc {

// Ensure this stays in sync with MainThreadScrollingReason in histograms.xml.
struct MainThreadScrollingReason {
  // Non-transient scrolling reasons.
  enum : uint32_t { kNotScrollingOnMain = 0 };
  enum : uint32_t { kHasBackgroundAttachmentFixedObjects = 1 << 0 };
  enum : uint32_t { kHasNonLayerViewportConstrainedObjects = 1 << 1 };
  enum : uint32_t { kThreadedScrollingDisabled = 1 << 2 };
  enum : uint32_t { kScrollbarScrolling = 1 << 3 };
  enum : uint32_t { kPageOverlay = 1 << 4 };
  // The maximum value reachable as a combination of the non-transient scrolling
  // reasons.
  enum : uint32_t { kMaxNonTransientScrollingReasons = (1 << 5) - 1 };

  // Transient scrolling reasons. These are computed for each scroll begin.
  enum : uint32_t { kNonFastScrollableRegion = 1 << 5 };
  enum : uint32_t { kEventHandlers = 1 << 6 };
  enum : uint32_t { kFailedHitTest = 1 << 7 };
  enum : uint32_t { kNoScrollingLayer = 1 << 8 };
  enum : uint32_t { kNotScrollable = 1 << 9 };
  enum : uint32_t { kContinuingMainThreadScroll = 1 << 10 };
  enum : uint32_t { kNonInvertibleTransform = 1 << 11 };
  enum : uint32_t { kPageBasedScrolling = 1 << 12 };

  // The number of flags in this struct (excluding itself).
  enum : uint32_t { kMainThreadScrollingReasonCount = 14 };
};

}  // namespace cc

#endif  // CC_INPUT_MAIN_THREAD_SCROLLING_REASON_H_
