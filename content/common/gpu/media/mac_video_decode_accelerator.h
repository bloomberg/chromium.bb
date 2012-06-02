// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_MAC_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_MAC_VIDEO_DECODE_ACCELERATOR_H_

#include <CoreVideo/CoreVideo.h>
#include <map>
#include <list>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "media/video/video_decode_accelerator.h"

namespace base {
class RefCountedBytes;
}
namespace gfx {
class GLContext;
class VideoDecodeAccelerationSupport;
}

class GpuCommandBufferStub;

class CONTENT_EXPORT MacVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  // Does not take ownership of |client| which must outlive |*this|.
  MacVideoDecodeAccelerator(media::VideoDecodeAccelerator::Client* client);

  // Set the OpenGL context to use.
  void SetCGLContext(CGLContextObj cgl_context);

  // Set extra data required to initialize the H.264 video decoder.
  // TODO(sail): Move this into Initialize.
  bool SetConfigInfo(uint32_t frame_width,
                     uint32_t frame_height,
                     const std::vector<uint8_t>& avc_data);

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

 private:
  virtual ~MacVideoDecodeAccelerator();

  // Callback for a completed frame.
  void OnFrameReady(int32 bitstream_buffer_id,
                    scoped_refptr<base::RefCountedBytes> bytes,
                    CVImageBufferRef image,
                    int status);

  // Sends available decoded frames to the client.
  void SendImages();

  // Stop the component when any error is detected.
  void StopOnError(media::VideoDecodeAccelerator::Error error);

  // Calls the client's initialize completed callback.
  void NotifyInitializeDone();

  // Requests pictures from the client.
  void RequestPictures();

  // Calls the client's flush completed callback.
  void NotifyFlushDone();

  // Calls the client's reset completed callback.
  void NotifyResetDone();

  // Notifies the client that the input buffer identifed by |input_buffer_id|
  // has been processed.
  void NotifyInputBufferRead(int input_buffer_id);

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;
  // C++ wrapper around the Mac VDA API.
  scoped_refptr<gfx::VideoDecodeAccelerationSupport> vda_support_;
  // Picture buffers that are available for use by the decoder to draw decoded
  // video frames on.
  std::list<media::PictureBuffer> available_pictures_;

  // Maps ids to picture buffers that are in use by the client.
  struct UsedPictureInfo {
    UsedPictureInfo(const media::PictureBuffer& pic,
                    const base::mac::ScopedCFTypeRef<CVImageBufferRef>& image);
    ~UsedPictureInfo();
    const media::PictureBuffer picture_buffer;
    const base::mac::ScopedCFTypeRef<CVImageBufferRef> image;
  };
  std::map<int32, UsedPictureInfo> used_pictures_;

  // List of decoded images waiting to be sent to the client.
  struct DecodedImageInfo {
    DecodedImageInfo();
    ~DecodedImageInfo();
    base::mac::ScopedCFTypeRef<CVImageBufferRef> image;
    int32 bitstream_buffer_id;
  };
  std::list<DecodedImageInfo> decoded_images_;

  // The context to use to perform OpenGL operations.
  CGLContextObj cgl_context_;

  // The number of bytes used to store the frame buffer size.
  size_t nalu_len_field_size_;

  // Size of a video frame.
  gfx::Size frame_size_;

  // Flag to check if pictures have been requested from the client.
  bool did_request_pictures_;
};

#endif  // CONTENT_COMMON_GPU_MEDIA_VIDEO_DECODE_ACCELERATOR_MAC_H_
