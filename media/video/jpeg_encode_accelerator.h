// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_JPEG_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_JPEG_ENCODE_ACCELERATOR_H_

#include <stdint.h>

#include "media/base/bitstream_buffer.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace media {

// JPEG encoder interface.
class MEDIA_EXPORT JpegEncodeAccelerator {
 public:
  enum Status {
    ENCODE_OK,

    HW_JPEG_ENCODE_NOT_SUPPORTED,

    // Eg. creation of encoder thread failed.
    THREAD_CREATION_FAILED,

    // Invalid argument was passed to an API method, e.g. the format of
    // VideoFrame is not supported.
    INVALID_ARGUMENT,

    // Output buffer is inaccessible, e.g. failed to map on another process.
    INACCESSIBLE_OUTPUT_BUFFER,

    // Failed to parse the incoming YUV image.
    PARSE_IMAGE_FAILED,

    // A fatal failure occurred in the GPU process layer or one of its
    // dependencies. Examples of such failures include hardware failures,
    // driver failures, library failures, and so on. Client is responsible for
    // destroying JEA after receiving this.
    PLATFORM_FAILURE,

    // Largest used enum. This should be adjusted when new errors are added.
    LARGEST_ERROR_ENUM = PLATFORM_FAILURE,
  };

  class MEDIA_EXPORT Client {
   public:
    // Callback called after each successful Encode().
    // Parameters:
    //  |video_frame_id| is |VideoFrame.unique_id()| of the input VideoFrame in
    //  corresponding Encode() call.
    //  |encoded_picture_size| is the actual size of encoded JPEG image in
    //  the BitstreamBuffer provided through encode().
    virtual void VideoFrameReady(int video_frame_id,
                                 size_t encoded_picture_size) = 0;

    // Callback to notify errors. Client is responsible for destroying JEA when
    // receiving a fatal error, i.e. PLATFORM_FAILURE. For other errors, client
    // is informed about the buffer that failed to encode and may continue
    // using the same instance of JEA.
    // Parameters:
    //  |video_frame_id| is |video_frame_id.unique_id()| of the input VideoFrame
    //  that resulted in the error.
    //  |status| would be one of the values of Status except ENCODE_OK.
    virtual void NotifyError(int video_frame_id, Status status) = 0;

   protected:
    virtual ~Client() {}
  };

  // Destroys the encoder: all pending inputs are dropped immediately. This
  // call may asynchronously free system resources, but its client-visible
  // effects are synchronous. After destructor returns, no more callbacks
  // will be made on the client.
  virtual ~JpegEncodeAccelerator() {}

  // Initializes the JPEG encoder. Should be called once per encoder
  // construction. This call is synchronous and returns ENCODE_OK iff
  // initialization is successful.
  // Parameters:
  //  |client| is the Client interface for encode callback. The provided
  //  pointer must be valid until destructor is called.
  virtual Status Initialize(Client* client) = 0;

  // Gets the maximum possible encoded result size.
  virtual size_t GetMaxCodedBufferSize(const gfx::Size& picture_size) = 0;

  // Encodes the given |video_frame| that contains a YUV image. Client will
  // receive the encoded result in Client::VideoFrameReady() callback with the
  // corresponding |unique_id_| in |video_frame|, or receive
  // Client::NotifyError() callback.
  // Parameters:
  //  |video_frame| contains the YUV image to be encoded.
  //  |quality| of JPEG image.
  //  |bitstream_buffer| that contains output buffer for encoded result. Clients
  //  should call GetMaxCodedBufferSize() and allocate the buffer accordingly.
  //  The buffer needs to be valid until VideoFrameReady() or NotifyError() is
  //  called.
  virtual void Encode(scoped_refptr<media::VideoFrame> video_frame,
                      int quality,
                      const BitstreamBuffer& bitstream_buffer) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_JPEG_ENCODE_ACCELERATOR_H_
