// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_RESOLUTION_CHOOSER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CAPTURE_RESOLUTION_CHOOSER_H_

#include "content/common/content_export.h"
#include "media/base/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// Encapsulates the logic that determines the capture frame resolution based on:
//   1. The configured maximum frame resolution and resolution change policy.
//   2. The resolution of the source content.
//   3. The current capabilities of the end-to-end system, in terms of the
//      maximum number of pixels per frame.
class CONTENT_EXPORT CaptureResolutionChooser {
 public:
  // media::ResolutionChangePolicy determines whether the variable frame
  // resolutions being computed must adhere to a fixed aspect ratio or not, or
  // that there must only be a single fixed resolution.
  CaptureResolutionChooser(
      const gfx::Size& max_frame_size,
      media::ResolutionChangePolicy resolution_change_policy);
  ~CaptureResolutionChooser();

  // Returns the current capture frame resolution to use.
  gfx::Size capture_size() const {
    return capture_size_;
  }

  // Updates the capture size based on a change in the resolution of the source
  // content.
  void SetSourceSize(const gfx::Size& source_size);

 private:
  // Called after any update that requires |capture_size_| be re-computed.
  void RecomputeCaptureSize();

  // Hard constraints.
  const gfx::Size max_frame_size_;
  const gfx::Size min_frame_size_;  // Computed from the ctor arguments.

  // Specifies the set of heuristics to use.
  const media::ResolutionChangePolicy resolution_change_policy_;

  // The capture frame resolution to use, ignoring the limitations imposed by
  // the capability metric.
  gfx::Size constrained_size_;

  // The current computed capture frame resolution.
  gfx::Size capture_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_RESOLUTION_CHOOSER_H_
