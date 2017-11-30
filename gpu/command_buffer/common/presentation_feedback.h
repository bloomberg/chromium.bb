// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PRESENTAION_FEEDBACK_H_
#define UI_GFX_PRESENTAION_FEEDBACK_H_

#include "base/time/time.h"

namespace gfx {

// Bitmask of flags in |PresentationCallback|. See
// //services/viz/public/interfaces/compositing/compositor_frame_sink.mojom
// for detail.
enum class PresentationFlags {
  VSYNC = 1 << 0,  // The presentation was synchronized to VSYNC.
  HW_CLOCK =
      1 << 1,  // The presentation |timestamp| is converted from hardware clock
               // by driver. Sampling a clock in user space is not acceptable
               // for this flag.
  HW_COMPLETION = 1 << 2,  // The display hardware signalled that it started
                           // using the new content. The opposite of this is
                           // e.g. a timer being used to guess when the
                           // display hardware has switched to the new image
                           // content.
  ZERO_COPY = 1 << 3,  // The presentation of this update was done zero-copy.
                       // Possible zero-copy cases include direct scanout of a
                       // fullscreen surface and a surface on a hardware
                       // overlay.
};

struct PresentationFeedback {
  PresentationFeedback() = default;
  PresentationFeedback(base::TimeTicks timestamp,
                       base::TimeDelta refresh,
                       uint32_t flags)
      : timestamp(timestamp), refresh(refresh), flags(flags) {}

  // The time when the buffer was or will be presented on screen.
  base::TimeTicks timestamp;

  // The time till the next refresh.
  base::TimeDelta refresh;

  // A combination of |PresentationFlags|.
  uint32_t flags = 0;
};

}  // namespace gfx

#endif  // UI_GFX_PRESENTAION_FEEDBACK_H_
