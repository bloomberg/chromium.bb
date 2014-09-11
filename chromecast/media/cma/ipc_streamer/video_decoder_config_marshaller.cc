// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/video_decoder_config_marshaller.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxExtraDataSize = 16 * 1024;

class SizeMarshaller {
 public:
  static void Write(const gfx::Size& size, MediaMessage* msg) {
    CHECK(msg->WritePod(size.width()));
    CHECK(msg->WritePod(size.height()));
  }

  static gfx::Size Read(MediaMessage* msg) {
    int w, h;
    CHECK(msg->ReadPod(&w));
    CHECK(msg->ReadPod(&h));
    return gfx::Size(w, h);
  }
};

class RectMarshaller {
 public:
  static void Write(const gfx::Rect& rect, MediaMessage* msg) {
    CHECK(msg->WritePod(rect.x()));
    CHECK(msg->WritePod(rect.y()));
    CHECK(msg->WritePod(rect.width()));
    CHECK(msg->WritePod(rect.height()));
  }

  static gfx::Rect Read(MediaMessage* msg) {
    int x, y, w, h;
    CHECK(msg->ReadPod(&x));
    CHECK(msg->ReadPod(&y));
    CHECK(msg->ReadPod(&w));
    CHECK(msg->ReadPod(&h));
    return gfx::Rect(x, y, w, h);
  }
};

}  // namespace

// static
void VideoDecoderConfigMarshaller::Write(
    const ::media::VideoDecoderConfig& config, MediaMessage* msg) {
  CHECK(msg->WritePod(config.codec()));
  CHECK(msg->WritePod(config.profile()));
  CHECK(msg->WritePod(config.format()));
  SizeMarshaller::Write(config.coded_size(), msg);
  RectMarshaller::Write(config.visible_rect(), msg);
  SizeMarshaller::Write(config.natural_size(), msg);
  CHECK(msg->WritePod(config.is_encrypted()));
  CHECK(msg->WritePod(config.extra_data_size()));
  if (config.extra_data_size() > 0)
    CHECK(msg->WriteBuffer(config.extra_data(), config.extra_data_size()));
}

// static
::media::VideoDecoderConfig VideoDecoderConfigMarshaller::Read(
    MediaMessage* msg) {
  ::media::VideoCodec codec;
  ::media::VideoCodecProfile profile;
  ::media::VideoFrame::Format format;
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  bool is_encrypted;
  size_t extra_data_size;
  scoped_ptr<uint8[]> extra_data;

  CHECK(msg->ReadPod(&codec));
  CHECK(msg->ReadPod(&profile));
  CHECK(msg->ReadPod(&format));
  coded_size = SizeMarshaller::Read(msg);
  visible_rect = RectMarshaller::Read(msg);
  natural_size = SizeMarshaller::Read(msg);
  CHECK(msg->ReadPod(&is_encrypted));
  CHECK(msg->ReadPod(&extra_data_size));

  CHECK_GE(codec, ::media::kUnknownVideoCodec);
  CHECK_LE(codec, ::media::kVideoCodecMax);
  CHECK_GE(profile, ::media::VIDEO_CODEC_PROFILE_UNKNOWN);
  CHECK_LE(profile, ::media::VIDEO_CODEC_PROFILE_MAX);
  CHECK_GE(format, ::media::VideoFrame::UNKNOWN);
  CHECK_LE(format, ::media::VideoFrame::FORMAT_MAX);
  CHECK_LT(extra_data_size, kMaxExtraDataSize);
  if (extra_data_size > 0) {
    extra_data.reset(new uint8[extra_data_size]);
    CHECK(msg->ReadBuffer(extra_data.get(), extra_data_size));
  }

  return ::media::VideoDecoderConfig(
      codec, profile, format,
      coded_size, visible_rect, natural_size,
      extra_data.get(), extra_data_size,
      is_encrypted);
}

}  // namespace media
}  // namespace chromecast
