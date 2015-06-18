// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace skia_runner {

class SkPictureRasterizer {
 public:
  SkPictureRasterizer(skia::RefPtr<GrContext> gr_context, int max_texture_size);
  ~SkPictureRasterizer();

  skia::RefPtr<SkImage> Rasterize(const SkPicture* picture) const;

  void set_msaa_sample_count(int msaa_sample_count) {
    msaa_sample_count_ = msaa_sample_count;
  }
  void set_use_lcd_text(bool use_lcd_text) { use_lcd_text_ = use_lcd_text; }
  void set_use_distance_field_text(bool use_distance_field_text) {
    use_distance_field_text_ = use_distance_field_text;
  }

 private:
  skia::RefPtr<SkImage> RasterizeTile(const SkPicture* picture,
                                      const SkRect& rect) const;

  bool use_lcd_text_;
  bool use_distance_field_text_;
  int msaa_sample_count_;

  int max_texture_size_;
  skia::RefPtr<GrContext> gr_context_;
};

}  // namespace skia_runner
