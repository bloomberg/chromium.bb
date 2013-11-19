// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session.h"

#include "base/message_loop/message_loop_proxy.h"
#include "chrome/renderer/media/cast_session_delegate.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"

CastSession::CastSession()
    : delegate_(new CastSessionDelegate()),
      io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()) {
}

CastSession::~CastSession() {
  // We should always be able to delete the object on the IO thread.
  CHECK(io_message_loop_proxy_->DeleteSoon(FROM_HERE, delegate_.release()));
}

void CastSession::StartAudio(const media::cast::AudioSenderConfig& config) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&CastSessionDelegate::StartAudio,
                 base::Unretained(delegate_.get()),
                 config));
}

void CastSession::StartVideo(const media::cast::VideoSenderConfig& config) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&CastSessionDelegate::StartVideo,
                 base::Unretained(delegate_.get()),
                 config));
}
