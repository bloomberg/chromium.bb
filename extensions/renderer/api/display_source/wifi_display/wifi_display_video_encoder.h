// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_VIDEO_ENCODER_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_VIDEO_ENCODER_H_

#include <string>

#include "base/memory/shared_memory.h"
#include "base/stl_util.h"
#include "media/base/video_frame.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/wds/src/libwds/public/video_format.h"

namespace extensions {

// This structure represents an encoded video frame.
struct WiFiDisplayEncodedFrame {
  const uint8_t* bytes() const {
    return reinterpret_cast<uint8_t*>(
        string_as_array(const_cast<std::string*>(&data)));
  }
  uint8_t* mutable_bytes() {
    return reinterpret_cast<uint8_t*>(string_as_array(&data));
  }

  base::TimeTicks pts;  // Presentation timestamp.
  base::TimeTicks dts;  // Decoder timestamp.
  std::string data;
  bool key_frame;
};

// This interface represents H.264 video encoder used by the
// Wi-Fi Display media pipeline.
// Threading: the client code should belong to a single thread.
class WiFiDisplayVideoEncoder :
    public base::RefCountedThreadSafe<WiFiDisplayVideoEncoder> {
 public:
  using EncodedFrameCallback =
      base::Callback<void(const WiFiDisplayEncodedFrame&)>;

  using VideoEncoderCallback =
      base::Callback<void(scoped_refptr<WiFiDisplayVideoEncoder>)>;

  using ReceiveVideoEncodeAcceleratorCallback =
      base::Callback<void(scoped_refptr<base::SingleThreadTaskRunner>,
      std::unique_ptr<media::VideoEncodeAccelerator>)>;
  using CreateVideoEncodeAcceleratorCallback =
      base::Callback<void(const ReceiveVideoEncodeAcceleratorCallback&)>;

  using ReceiveEncodeMemoryCallback =
      base::Callback<void(std::unique_ptr<base::SharedMemory>)>;
  using CreateEncodeMemoryCallback =
      base::Callback<void(size_t size, const ReceiveEncodeMemoryCallback&)>;

  struct InitParameters {
    InitParameters();
    InitParameters(const InitParameters&);
    ~InitParameters();
    gfx::Size frame_size;
    int frame_rate;
    int bit_rate;
    wds::H264Profile profile;
    wds::H264Level level;
    // VEA-specific parameters.
    CreateEncodeMemoryCallback create_memory_callback;
    CreateVideoEncodeAcceleratorCallback vea_create_callback;
  };

  // A factory method that creates a new encoder instance from the given
  // |params|, the encoder instance is returned as an argument of
  // |result_callback| ('nullptr' argument means encoder creation failure).
  static void Create(
      const InitParameters& params,
      const VideoEncoderCallback& encoder_callback);

  // Encodes the given raw frame. The resulting encoded frame is passed
  // as an |encoded_callback|'s argument which is set via 'SetCallbacks'
  // method.
  virtual void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      base::TimeTicks reference_time) = 0;

  // Requests the next encoded frame to be an instantaneous decoding refresh
  // (IDR) picture.
  virtual void RequestIDRPicture() = 0;

  // Sets callbacks for the obtained encoder instance:
  // |encoded_callback| is invoked to return the next encoded frame
  // |error_callback| is invoked to report a fatal encoder error
  void SetCallbacks(const EncodedFrameCallback& encoded_callback,
                    const base::Closure& error_callback);

 protected:
  friend class base::RefCountedThreadSafe<WiFiDisplayVideoEncoder>;
  WiFiDisplayVideoEncoder();
  virtual ~WiFiDisplayVideoEncoder();

  EncodedFrameCallback encoded_callback_;
  base::Closure error_callback_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_VIDEO_ENCODER_H_
