// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_
#define CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_

#include "base/macros.h"
#include "content/browser/compositor/browser_compositor_overlay_candidate_validator.h"
#include "content/common/content_export.h"

namespace content {

// An overlay validator for supporting fullscreen video underlays on Android.
// Things are a bit different on Android compared with other platforms. By the
// time a video frame is marked as overlayable it means the video decoder was
// outputting to a Surface that we can't read back from. As a result, the
// overlay must always succeed, or the video won't be visible. This is one of of
// the reasons that only fullscreen is supported: we have to be sure that
// nothing will cause the overlay to be rejected, because there's no fallback to
// gl compositing.
class CONTENT_EXPORT BrowserCompositorOverlayCandidateValidatorAndroid
    : public BrowserCompositorOverlayCandidateValidator {
 public:
  BrowserCompositorOverlayCandidateValidatorAndroid();
  ~BrowserCompositorOverlayCandidateValidatorAndroid() override;

  void GetStrategies(cc::OverlayProcessor::StrategyList* strategies) override;
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;
  bool AllowCALayerOverlays() override;

  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOverlayCandidateValidatorAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_ANDROID_H_
