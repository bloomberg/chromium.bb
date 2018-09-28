// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/video_frame_thumbnail_converter.h"

#include "base/macros.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/gl_helper.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"

namespace {

// Class to generate bitmap based on video frame backed by texture in remote
// process.
class TextureFrameThumbnailConverter : public VideoFrameThumbnailConverter {
 public:
  TextureFrameThumbnailConverter(
      scoped_refptr<ws::ContextProviderCommandBuffer> context_provider)
      : context_provider_(std::move(context_provider)) {}
  TextureFrameThumbnailConverter() = default;
  ~TextureFrameThumbnailConverter() override = default;

 private:
  // VideoFrameThumbnailConverter implementation.
  void ConvertToBitmap(const scoped_refptr<media::VideoFrame>& frame,
                       BitmapCallback pixel_callback) override {
    DCHECK(frame->HasTextures())
        << "This implementation only do texture read backs.";

    // Read back the texture data contained in the video frame.
    media::PaintCanvasVideoRenderer renderer;
    SkBitmap skbitmap;
    skbitmap.allocN32Pixels(frame->visible_rect().width(),
                            frame->visible_rect().height());
    cc::SkiaPaintCanvas canvas(skbitmap);
    renderer.Copy(frame, &canvas,
                  media::Context3D(context_provider_->ContextGL(),
                                   context_provider_->GrContext()));

    std::move(pixel_callback).Run(true, std::move(skbitmap));
  }

  // Command buffer channel used to read back the pixel data from texture in
  // remote process.
  scoped_refptr<ws::ContextProviderCommandBuffer> context_provider_;

  DISALLOW_COPY_AND_ASSIGN(TextureFrameThumbnailConverter);
};

}  // namespace

// static
std::unique_ptr<VideoFrameThumbnailConverter>
VideoFrameThumbnailConverter::Create(
    media::VideoCodec codec,
    scoped_refptr<ws::ContextProviderCommandBuffer> context_provider) {
  // TODO(xingliu): Implement vpx decode and render pipeline.
  DCHECK_NE(codec, media::VideoCodec::kCodecVP8);
  DCHECK_NE(codec, media::VideoCodec::kCodecVP9);

  return std::make_unique<TextureFrameThumbnailConverter>(
      std::move(context_provider));
}
