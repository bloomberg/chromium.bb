// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/cast_config.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace media {
namespace cast {
class CastEnvironment;
class CastSender;
}  // namespace cast
}  // namespace media

// This class hosts CastSender and connects it to audio/video frame input
// and network socket.
// This class is created on the render thread and destroyed on the IO
// thread. All methods are accessible only on the IO thread.
class CastSessionDelegate {
 public:
  CastSessionDelegate();
  ~CastSessionDelegate();

  // After calling StartAudio() and StartVideo() with configuration encoding
  // will begin.
  void StartAudio(const media::cast::AudioSenderConfig& config);
  void StartVideo(const media::cast::VideoSenderConfig& config);

 private:
  // Start encoding threads and configure CastSender. It is ready to accept
  // audio/video frames after this call.
  void StartSending();

  // If both audio and video are configured properly then start CastSender.
  void MaybeStartSending();

  base::ThreadChecker thread_checker_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  scoped_ptr<media::cast::CastSender> cast_sender_;

  // Utilities threads owned by this class. They are used by CastSender for
  // encoding.
  // TODO(hclam): See crbug.com/317006 for more details.
  // This class shouldn't create and own threads.
  base::Thread audio_encode_thread_;
  base::Thread video_encode_thread_;

  // Clock used by CastSender.
  base::DefaultTickClock clock_;

  // Configuration for audio and video.
  media::cast::AudioSenderConfig audio_config_;
  media::cast::VideoSenderConfig video_config_;
  bool audio_configured_;
  bool video_configured_;

  // Proxy to the IO message loop.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
