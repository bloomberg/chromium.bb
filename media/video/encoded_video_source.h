// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_ENCODED_VIDEO_SOURCE_H_
#define MEDIA_VIDEO_ENCODED_VIDEO_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "media/base/encoded_bitstream_buffer.h"
#include "media/video/video_encode_types.h"

namespace media {

// Class to represent any encoded video source. Anything that provides encoded
// video can be an EncodedVideoSource. Notable examples of this can be video
// encoder and webcam that has encoding capabilities.
// TODO(hshi): merge this with VEA interface. http://crbug.com/248334.
class EncodedVideoSource {
 public:
  class Client {
   public:
    // Notifies client that bitstream is opened successfully. The |params|
    // contains the actual encoding parameters chosen by the browser process.
    // It may be different from the params requested in OpenBitstream().
    virtual void OnOpened(const VideoEncodingParameters& params) = 0;

    // Notifies client that bitstream is closed. After this call it is
    // guaranteed that client will not receive further calls.
    virtual void OnClosed() = 0;

    // Delivers an encoded bitstream buffer to the client.
    virtual void OnBufferReady(
        scoped_refptr<const EncodedBitstreamBuffer> buffer) = 0;

    // Notifies client that encoding parameters has changed. The |params|
    // contains the current encoding parameters chosen by the browser process.
    // It may be different from the params requested in TrySetBitstreamConfig().
    virtual void OnConfigChanged(
        const RuntimeVideoEncodingParameters& params) = 0;
  };

  // Callback is invoked once RequestCapabilities() is complete.
  typedef base::Callback<void(const VideoEncodingCapabilities& capabilities)>
      RequestCapabilitiesCallback;

  // RequestCapabilities initiates an asynchronous query for the types of
  // encoded bitstream supported by the encoder. This call should be invoked
  // only once. EncodedVideoSource will invoke |callback| when capabilities
  // become available.
  virtual void RequestCapabilities(
      const RequestCapabilitiesCallback& callback) = 0;

  // OpenBitstream opens the bitstream on the encoded video source. Only one
  // bitstream can be opened for an encoded video source.
  virtual void OpenBitstream(Client* client,
                             const VideoEncodingParameters& params) = 0;

  // CloseBitstream closes the bitstream.
  virtual void CloseBitstream() = 0;

  // ReturnBitstreamBuffer notifies that the data within the buffer has been
  // processed and it can be reused to encode upcoming bitstream.
  virtual void ReturnBitstreamBuffer(
      scoped_refptr<const media::EncodedBitstreamBuffer> buffer) = 0;

  // TrySetBitstreamConfig requests to change encoding parameters. Old config
  // must be considered valid until OnConfigChanged is invoked on the client
  // signaling successful change.
  virtual void TrySetBitstreamConfig(
      const RuntimeVideoEncodingParameters& params) = 0;

  // RequestKeyFrame requests a key frame.
  virtual void RequestKeyFrame() = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_ENCODED_VIDEO_SOURCE_H_

