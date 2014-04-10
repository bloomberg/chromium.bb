// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_H_
#define MEDIA_BASE_VIDEO_FRAME_H_

#include "base/callback.h"
#include "base/md5.h"
#include "base/memory/shared_memory.h"
#include "media/base/buffers.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace media {

class MEDIA_EXPORT VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  enum {
    kFrameSizeAlignment = 16,
    kFrameSizePadding = 16,
    kFrameAddressAlignment = 32
  };

  enum {
    kMaxPlanes = 4,

    kYPlane = 0,
    kUPlane = 1,
    kVPlane = 2,
    kAPlane = 3,
  };

  // Surface formats roughly based on FOURCC labels, see:
  // http://www.fourcc.org/rgb.php
  // http://www.fourcc.org/yuv.php
  // Logged to UMA, so never reuse values.
  enum Format {
    UNKNOWN = 0,  // Unknown format value.
    YV12 = 1,  // 12bpp YVU planar 1x1 Y, 2x2 VU samples
    YV16 = 2,  // 16bpp YVU planar 1x1 Y, 2x1 VU samples
    I420 = 3,  // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
    YV12A = 4,  // 20bpp YUVA planar 1x1 Y, 2x2 VU, 1x1 A samples.
#if defined(VIDEO_HOLE)
    HOLE = 5,  // Hole frame.
#endif  // defined(VIDEO_HOLE)
    NATIVE_TEXTURE = 6,  // Native texture.  Pixel-format agnostic.
    YV12J = 7,  // JPEG color range version of YV12
    FORMAT_MAX = YV12J,  // Must always be equal to largest entry logged.
  };

  // Returns the name of a Format as a string.
  static std::string FormatToString(Format format);

  // Creates a new frame in system memory with given parameters. Buffers for
  // the frame are allocated but not initialized.
  static scoped_refptr<VideoFrame> CreateFrame(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Call prior to CreateFrame to ensure validity of frame configuration. Called
  // automatically by VideoDecoderConfig::IsValidConfig().
  // TODO(scherkus): VideoDecoderConfig shouldn't call this method
  static bool IsValidConfig(Format format, const gfx::Size& coded_size,
                            const gfx::Rect& visible_rect,
                            const gfx::Size& natural_size);

  // CB to write pixels from the texture backing this frame into the
  // |const SkBitmap&| parameter.
  typedef base::Callback<void(const SkBitmap&)> ReadPixelsCB;

  // CB to be called on the mailbox backing this frame when the frame is
  // destroyed.
  typedef base::Callback<void(scoped_ptr<gpu::MailboxHolder>)> ReleaseMailboxCB;

  // Wraps a native texture of the given parameters with a VideoFrame.  The
  // backing of the VideoFrame is held in the mailbox held by |mailbox_holder|,
  // and |mailbox_holder_release_cb| will be called with |mailbox_holder| as the
  // argument when the VideoFrame is to be destroyed.
  // |read_pixels_cb| may be used to do (slow!) readbacks from the
  // texture to main memory.
  static scoped_refptr<VideoFrame> WrapNativeTexture(
      scoped_ptr<gpu::MailboxHolder> mailbox_holder,
      const ReleaseMailboxCB& mailbox_holder_release_cb,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      const ReadPixelsCB& read_pixels_cb);

  // Read pixels from the native texture backing |*this| and write
  // them to |pixels| as BGRA.  |pixels| must point to a buffer at
  // least as large as 4 * visible_rect().size().GetArea().
  void ReadPixelsFromNativeTexture(const SkBitmap& pixels);

  // Wraps packed image data residing in a memory buffer with a VideoFrame.
  // The image data resides in |data| and is assumed to be packed tightly in a
  // buffer of logical dimensions |coded_size| with the appropriate bit depth
  // and plane count as given by |format|.  The shared memory handle of the
  // backing allocation, if present, can be passed in with |handle|.  When the
  // frame is destroyed, |no_longer_needed_cb.Run()| will be called.
  static scoped_refptr<VideoFrame> WrapExternalPackedMemory(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      uint8* data,
      size_t data_size,
      base::SharedMemoryHandle handle,
      base::TimeDelta timestamp,
      const base::Closure& no_longer_needed_cb);

  // Wraps external YUV data of the given parameters with a VideoFrame.
  // The returned VideoFrame does not own the data passed in. When the frame
  // is destroyed |no_longer_needed_cb.Run()| will be called.
  // TODO(sheu): merge this into WrapExternalSharedMemory().
  // http://crbug.com/270217
  static scoped_refptr<VideoFrame> WrapExternalYuvData(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      int32 y_stride,
      int32 u_stride,
      int32 v_stride,
      uint8* y_data,
      uint8* u_data,
      uint8* v_data,
      base::TimeDelta timestamp,
      const base::Closure& no_longer_needed_cb);

  // Wraps |frame| and calls |no_longer_needed_cb| when the wrapper VideoFrame
  // gets destroyed. |visible_rect| must be a sub rect within
  // frame->visible_rect().
  static scoped_refptr<VideoFrame> WrapVideoFrame(
      const scoped_refptr<VideoFrame>& frame,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      const base::Closure& no_longer_needed_cb);

  // Creates a frame which indicates end-of-stream.
  static scoped_refptr<VideoFrame> CreateEOSFrame();

  // Allocates YV12 frame based on |size|, and sets its data to the YUV(y,u,v).
  static scoped_refptr<VideoFrame> CreateColorFrame(
      const gfx::Size& size,
      uint8 y, uint8 u, uint8 v,
      base::TimeDelta timestamp);

  // Allocates YV12 frame based on |size|, and sets its data to the YUV
  // equivalent of RGB(0,0,0).
  static scoped_refptr<VideoFrame> CreateBlackFrame(const gfx::Size& size);

#if defined(VIDEO_HOLE)
  // Allocates a hole frame.
  static scoped_refptr<VideoFrame> CreateHoleFrame(const gfx::Size& size);
#endif  // defined(VIDEO_HOLE)

  static size_t NumPlanes(Format format);

  // Returns the required allocation size for a (tightly packed) frame of the
  // given coded size and format.
  static size_t AllocationSize(Format format, const gfx::Size& coded_size);

  // Returns the plane size for a plane of the given coded size and format.
  static gfx::Size PlaneSize(Format format,
                             size_t plane,
                             const gfx::Size& coded_size);

  // Returns the required allocation size for a (tightly packed) plane of the
  // given coded size and format.
  static size_t PlaneAllocationSize(Format format,
                                    size_t plane,
                                    const gfx::Size& coded_size);

  Format format() const { return format_; }

  const gfx::Size& coded_size() const { return coded_size_; }
  const gfx::Rect& visible_rect() const { return visible_rect_; }
  const gfx::Size& natural_size() const { return natural_size_; }

  int stride(size_t plane) const;

  // Returns the number of bytes per row and number of rows for a given plane.
  //
  // As opposed to stride(), row_bytes() refers to the bytes representing
  // frame data scanlines (coded_size.width() pixels, without stride padding).
  int row_bytes(size_t plane) const;
  int rows(size_t plane) const;

  // Returns pointer to the buffer for a given plane. The memory is owned by
  // VideoFrame object and must not be freed by the caller.
  uint8* data(size_t plane) const;

  // Returns the mailbox holder of the native texture wrapped by this frame.
  // Only valid to call if this is a NATIVE_TEXTURE frame. Before using the
  // mailbox, the caller must wait for the included sync point.
  gpu::MailboxHolder* mailbox_holder() const;

  // Returns the shared-memory handle, if present
  base::SharedMemoryHandle shared_memory_handle() const;

  // Returns true if this VideoFrame represents the end of the stream.
  bool end_of_stream() const { return end_of_stream_; }

  base::TimeDelta timestamp() const {
    return timestamp_;
  }
  void set_timestamp(const base::TimeDelta& timestamp) {
    timestamp_ = timestamp;
  }

  // Used to keep a running hash of seen frames.  Expects an initialized MD5
  // context.  Calls MD5Update with the context and the contents of the frame.
  void HashFrameForTesting(base::MD5Context* context);

 private:
  friend class base::RefCountedThreadSafe<VideoFrame>;
  // Clients must use the static CreateFrame() method to create a new frame.
  VideoFrame(Format format,
             const gfx::Size& coded_size,
             const gfx::Rect& visible_rect,
             const gfx::Size& natural_size,
             base::TimeDelta timestamp,
             bool end_of_stream);
  virtual ~VideoFrame();

  void AllocateYUV();

  // Used to DCHECK() plane parameters.
  bool IsValidPlane(size_t plane) const;

  // Frame format.
  const Format format_;

  // Width and height of the video frame, in pixels. This must include pixel
  // data for the whole image; i.e. for YUV formats with subsampled chroma
  // planes, in the case that the visible portion of the image does not line up
  // on a sample boundary, |coded_size_| must be rounded up appropriately and
  // the pixel data provided for the odd pixels.
  const gfx::Size coded_size_;

  // Width, height, and offsets of the visible portion of the video frame. Must
  // be a subrect of |coded_size_|. Can be odd with respect to the sample
  // boundaries, e.g. for formats with subsampled chroma.
  const gfx::Rect visible_rect_;

  // Width and height of the visible portion of the video frame
  // (|visible_rect_.size()|) with aspect ratio taken into account.
  const gfx::Size natural_size_;

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32 strides_[kMaxPlanes];

  // Array of data pointers to each plane.
  uint8* data_[kMaxPlanes];

  // Native texture mailbox, if this is a NATIVE_TEXTURE frame.
  scoped_ptr<gpu::MailboxHolder> mailbox_holder_;
  ReleaseMailboxCB mailbox_holder_release_cb_;
  ReadPixelsCB read_pixels_cb_;

  // Shared memory handle, if this frame was allocated from shared memory.
  base::SharedMemoryHandle shared_memory_handle_;

  base::Closure no_longer_needed_cb_;

  base::TimeDelta timestamp_;

  const bool end_of_stream_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoFrame);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_H_
