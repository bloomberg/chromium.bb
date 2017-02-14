// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_SKCANVAS_VIDEO_RENDERER_H_
#define MEDIA_RENDERERS_SKCANVAS_VIDEO_RENDERER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_rotation.h"
#include "media/filters/context_3d.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"


namespace gfx {
class RectF;
}

namespace media {

// TODO(enne): rename to PaintCanvasVideoRenderer
// Handles rendering of VideoFrames to PaintCanvases.
class MEDIA_EXPORT SkCanvasVideoRenderer {
 public:
  SkCanvasVideoRenderer();
  ~SkCanvasVideoRenderer();

  // Paints |video_frame| on |canvas|, scaling and rotating the result to fit
  // dimensions specified by |dest_rect|.
  // If the format of |video_frame| is PIXEL_FORMAT_NATIVE_TEXTURE, |context_3d|
  // must be provided.
  //
  // Black will be painted on |canvas| if |video_frame| is null.
  void Paint(const scoped_refptr<VideoFrame>& video_frame,
             cc::PaintCanvas* canvas,
             const gfx::RectF& dest_rect,
             cc::PaintFlags& flags,
             VideoRotation video_rotation,
             const Context3D& context_3d);

  // Copy |video_frame| on |canvas|.
  // If the format of |video_frame| is PIXEL_FORMAT_NATIVE_TEXTURE, |context_3d|
  // must be provided.
  void Copy(const scoped_refptr<VideoFrame>& video_frame,
            cc::PaintCanvas* canvas,
            const Context3D& context_3d);

  // Convert the contents of |video_frame| to raw RGB pixels. |rgb_pixels|
  // should point into a buffer large enough to hold as many 32 bit RGBA pixels
  // as are in the visible_rect() area of the frame.
  static void ConvertVideoFrameToRGBPixels(const media::VideoFrame* video_frame,
                                           void* rgb_pixels,
                                           size_t row_bytes);

  // Copy the contents of texture of |video_frame| to texture |texture|.
  // |level|, |internal_format|, |type| specify target texture |texture|.
  // The format of |video_frame| must be VideoFrame::NATIVE_TEXTURE.
  // Assumes |texture| has already been allocated with the appropriate
  // size and a compatible format, internal format and type; this is
  // effectively a "TexSubImage" operation.
  static void CopyVideoFrameSingleTextureToGLTexture(
      gpu::gles2::GLES2Interface* gl,
      VideoFrame* video_frame,
      unsigned int texture,
      bool premultiply_alpha,
      bool flip_y);

  // Copy the contents of texture of |video_frame| to texture |texture| in
  // context |destination_gl|.
  // |level|, |internal_format|, |type| specify target texture |texture|.
  // The format of |video_frame| must be VideoFrame::NATIVE_TEXTURE.
  // |context_3d| has a GrContext that may be used during the copy.
  // Assumes |texture| has already been allocated with the appropriate
  // size and a compatible format, internal format and type; this is
  // effectively a "TexSubImage" operation.
  // Returns true on success.
  bool CopyVideoFrameTexturesToGLTexture(
      const Context3D& context_3d,
      gpu::gles2::GLES2Interface* destination_gl,
      const scoped_refptr<VideoFrame>& video_frame,
      unsigned int texture,
      bool premultiply_alpha,
      bool flip_y);

  // Converts unsigned 16-bit value to target |format| for Y16 format and
  // calls WebGL texImage2D.
  // |level|, |internal_format|, |format|, |type| are WebGL texImage2D
  // parameters.
  // Returns false if there is no implementation for given parameters.
  static bool TexImage2D(unsigned target,
                         gpu::gles2::GLES2Interface* gl,
                         VideoFrame* video_frame,
                         int level,
                         int internalformat,
                         unsigned format,
                         unsigned type,
                         bool flip_y,
                         bool premultiply_alpha);

  // Converts unsigned 16-bit value to target |format| for Y16 format and
  // calls WebGL texSubImage2D.
  // |level|, |format|, |type|, |xoffset| and |yoffset| are texSubImage2D
  // parameters.
  // Returns false if there is no implementation for given parameters.
  static bool TexSubImage2D(unsigned target,
                            gpu::gles2::GLES2Interface* gl,
                            VideoFrame* video_frame,
                            int level,
                            unsigned format,
                            unsigned type,
                            int xoffset,
                            int yoffset,
                            bool flip_y,
                            bool premultiply_alpha);

  // In general, We hold the most recently painted frame to increase the
  // performance for the case that the same frame needs to be painted
  // repeatedly. Call this function if you are sure the most recent frame will
  // never be painted again, so we can release the resource.
  void ResetCache();

  void CorrectLastImageDimensions(const SkIRect& visible_rect);

  // Used for unit test.
  SkISize LastImageDimensionsForTesting();

 private:
  // Update the cache holding the most-recently-painted frame. Returns false
  // if the image couldn't be updated.
  bool UpdateLastImage(const scoped_refptr<VideoFrame>& video_frame,
                       const Context3D& context_3d);

  // Last image used to draw to the canvas.
  sk_sp<SkImage> last_image_;
  // Timestamp of the videoframe used to generate |last_image_|.
  base::TimeDelta last_timestamp_ = media::kNoTimestamp;
  // If |last_image_| is not used for a while, it's deleted to save memory.
  base::DelayTimer last_image_deleting_timer_;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  // Used for unit test.
  SkISize last_image_dimensions_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(SkCanvasVideoRenderer);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_SKCANVAS_VIDEO_RENDERER_H_
