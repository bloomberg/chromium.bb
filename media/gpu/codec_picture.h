// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CODEC_PICTURE_H_
#define MEDIA_GPU_CODEC_PICTURE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// Represents a picture encoded (or to be encoded) with a video codec, such as
// VP8. Users of this class do not require knowledge of the codec format, or any
// codec-specific details related to the picture, but may otherwise need to pass
// or keep references to the picture, e.g. to keep a list of reference pictures
// required for a codec task valid until it is finished. Also used for storing
// non-codec-specific metadata.
class MEDIA_GPU_EXPORT CodecPicture
    : public base::RefCountedThreadSafe<CodecPicture> {
 public:
  CodecPicture() = default;

  int32_t bitstream_id() const { return bitstream_id_; }
  void set_bitstream_id(int32_t bitstream_id) { bitstream_id_ = bitstream_id; }

  const gfx::Rect visible_rect() const { return visible_rect_; }
  void set_visible_rect(const gfx::Rect& rect) { visible_rect_ = rect; }

 protected:
  friend class base::RefCountedThreadSafe<CodecPicture>;
  virtual ~CodecPicture() = default;

 private:
  int32_t bitstream_id_ = -1;
  gfx::Rect visible_rect_;

  DISALLOW_COPY_AND_ASSIGN(CodecPicture);
};

}  // namespace media

#endif  // MEDIA_GPU_CODEC_PICTURE_H_
