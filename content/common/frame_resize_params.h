// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_RESIZE_PARAMS_H_
#define CONTENT_COMMON_FRAME_RESIZE_PARAMS_H_

#include "content/common/content_export.h"
#include "content/public/common/screen_info.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// TODO(fsamuel): We might want to unify this with content::ResizeParams.
struct CONTENT_EXPORT FrameResizeParams {
  FrameResizeParams();
  FrameResizeParams(const FrameResizeParams& other);
  ~FrameResizeParams();

  FrameResizeParams& operator=(const FrameResizeParams& other);

  // Information about the screen (dpi, depth, etc..).
  ScreenInfo screen_info;

  // Whether or not blink should be in auto-resize mode.
  bool auto_resize_enabled;

  // The minimum size for Blink if auto-resize is enabled.
  gfx::Size min_size_for_auto_resize;

  // The maximum size for Blink if auto-resize is enabled.
  gfx::Size max_size_for_auto_resize;

  // This variable is increased after each auto-resize. If the
  // renderer receives a ResizeParams with stale auto_resize_seqence_number,
  // then the resize request is dropped.
  uint64_t auto_resize_sequence_number;

  gfx::Rect screen_space_rect;

  gfx::Size local_frame_size;
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_RESIZE_PARAMS_H_
