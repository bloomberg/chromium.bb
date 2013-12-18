// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_sender.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace media {
class VideoFrame;

namespace cast {
class CastEnvironment;
class FrameInput;
}  // namespace cast
}  // namespace media

// This class hosts CastSender and connects it to audio/video frame input
// and network socket.
// This class is created on the render thread and destroyed on the IO
// thread. All methods are accessible only on the IO thread.
class CastSessionDelegate : public media::cast::PacketSender {
 public:
  typedef
  base::Callback<void(const scoped_refptr<media::cast::FrameInput>&)>
  FrameInputAvailableCallback;
  typedef base::Callback<void(const std::vector<char>&)> SendPacketCallback;

  CastSessionDelegate();
  virtual ~CastSessionDelegate();

  // After calling StartAudio() and StartVideo() with configuration encoding
  // will begin.
  void StartAudio(const media::cast::AudioSenderConfig& config,
                  const FrameInputAvailableCallback& callback);
  void StartVideo(const media::cast::VideoSenderConfig& config,
                  const FrameInputAvailableCallback& callback);
  void StartSending(const SendPacketCallback& callback);
  void ReceivePacket(const std::vector<char>& packet);

 private:
  // Start encoding threads and configure CastSender. It is ready to accept
  // audio/video frames after this call.
  void StartSendingInternal();

  // media::cast::PacketSender Implementation
  virtual bool SendPacket(const media::cast::Packet& packet) OVERRIDE;
  virtual bool SendPackets(const media::cast::PacketList& packets) OVERRIDE;

 private:
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
  std::vector<FrameInputAvailableCallback> frame_input_available_callbacks_;
  SendPacketCallback send_packet_callback_;

  // Proxy to the IO message loop.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
