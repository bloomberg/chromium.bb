// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/omx_video_decoder.h"

#include "base/waitable_event.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/omx_video_decode_engine.h"

namespace media {

// static
FilterFactory* OmxVideoDecoder::CreateFactory() {
  return new FilterFactoryImpl1<OmxVideoDecoder, OmxVideoDecodeEngine*>(
      new OmxVideoDecodeEngine());
}

// static
bool OmxVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  if (!format.GetAsString(MediaFormat::kMimeType, &mime_type) ||
      mime_type::kFFmpegVideo != mime_type) {
    return false;
  }

  // TODO(ajwong): Find a good way to white-list formats that OpenMAX can
  // handle.
  int codec_id;
  if (format.GetAsInteger(MediaFormat::kFFmpegCodecID, &codec_id) &&
      codec_id == CODEC_ID_H264) {
    return true;
  }

  return false;
}

OmxVideoDecoder::OmxVideoDecoder(OmxVideoDecodeEngine* engine)
    : VideoDecoderImpl(engine),
      omx_engine_(engine) {
}

OmxVideoDecoder::~OmxVideoDecoder() {
}

void OmxVideoDecoder::set_message_loop(MessageLoop* message_loop) {
  // TODO(ajwong): Is there a way around needing to propogate the message loop?
  VideoDecoderImpl::set_message_loop(message_loop);
  omx_engine_->set_message_loop(message_loop);
}

void OmxVideoDecoder::Stop() {
  // TODO(ajwong): This is a total hack. Make async.
  base::WaitableEvent event(false, false);
  omx_engine_->Stop(NewCallback(&event, &base::WaitableEvent::Signal));
  event.Wait();
}

}  // namespace media
