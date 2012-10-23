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
#include "content/common/gpu/media/avc_config_record_builder.h"
#include "content/common/gpu/media/h264_parser.h"
#include "media/video/video_decode_accelerator.h"

namespace base {
class RefCountedBytes;
}
namespace gfx {
class GLContext;
class VideoDecodeAccelerationSupport;
}

namespace content {

class GpuCommandBufferStub;

class CONTENT_EXPORT MacVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  // Does not take ownership of |client| which must outlive |*this|.
  MacVideoDecodeAccelerator(CGLContextObj cgl_context,
                            media::VideoDecodeAccelerator::Client* client);
  virtual ~MacVideoDecodeAccelerator();

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

  // Callback for a completed frame.
  void OnFrameReady(int32 bitstream_buffer_id,
                    scoped_refptr<base::RefCountedBytes> bytes,
                    CVImageBufferRef image,
                    int status);

  // Sends available decoded frames to the client.
  void SendImages();

  // Stop the component when any error is detected.
  void StopOnError(media::VideoDecodeAccelerator::Error error);

  // Create the decoder.
  bool CreateDecoder(const std::vector<uint8_t>& extra_data);

  // Send the given NALU to the decoder.
  void DecodeNALU(const H264NALU& nalu, int32 bitstream_buffer_id);

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

  // Helper for Destroy(), doing all the actual work except for deleting self.
  void Cleanup();

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

  // Flag to check if AVC decoder configuration record has been built.
  bool did_build_config_record_;

  // Parser for the H264 stream.
  H264Parser h264_parser_;

  // Utility to build the AVC configuration record.
  AVCConfigRecordBuilder config_record_builder_;

  // Maps a bitstream ID to the number of NALUs that are being decoded for
  // that bitstream. This is used to ensure that NotifyEndOfBitstreamBuffer()
  // is called after all NALUs contained in a bitstream have been decoded.
  std::map<int32, int> bitstream_nalu_count_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VIDEO_DECODE_ACCELERATOR_MAC_H_
