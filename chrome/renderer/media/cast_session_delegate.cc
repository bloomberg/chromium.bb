// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"

using media::cast::AudioSenderConfig;
using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::VideoSenderConfig;

CastSessionDelegate::CastSessionDelegate()
    : audio_encode_thread_("CastAudioEncodeThread"),
      video_encode_thread_("CastVideoEncodeThread"),
      io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()) {
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
}

void CastSessionDelegate::PrepareForSending() {
  DCHECK(base::MessageLoopProxy::current() == io_message_loop_proxy_);

  if (cast_environment_)
    return;

  audio_encode_thread_.Start();
  video_encode_thread_.Start();

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // There's no need to decode so no thread assigned for decoding.
  cast_environment_ = new CastEnvironment(
      &clock_,
      base::MessageLoopProxy::current(),
      audio_encode_thread_.message_loop_proxy(),
      NULL,
      video_encode_thread_.message_loop_proxy(),
      NULL);

  // TODO(hclam): A couple things need to be done here:
  // 1. Pass audio and video configuration to CastSender.
  // 2. Connect media::cast::PacketSender to net::Socket interface.
  // 3. Implement VideoEncoderController to configure hardware encoder.
  cast_sender_.reset(CastSender::CreateCastSender(
      cast_environment_,
      AudioSenderConfig(),
      VideoSenderConfig(),
      NULL,
      NULL));
}
