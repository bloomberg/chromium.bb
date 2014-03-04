// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
class EncodingEventSubscriber;
class FrameInput;

namespace transport {
class CastTransportSender;
}  // namespace transport
}  // namespace cast
}  // namespace media

// This class hosts CastSender and connects it to audio/video frame input
// and network socket.
// This class is created on the render thread and destroyed on the IO
// thread. All methods are accessible only on the IO thread.
class CastSessionDelegate {
 public:
  typedef base::Callback<void(const scoped_refptr<media::cast::FrameInput>&)>
      FrameInputAvailableCallback;
  typedef base::Callback<void(scoped_ptr<std::string>)> EventLogsCallback;

  CastSessionDelegate();
  virtual ~CastSessionDelegate();

  // After calling StartAudio() or StartVideo() encoding of that media will
  // begin as soon as data is delivered to its sink, if the second method is
  // called the first media will be restarted. It is strongly recommended not to
  // deliver any data between calling the two methods.
  // It's OK to call only one of the two methods.
  void StartAudio(const media::cast::AudioSenderConfig& config,
                  const FrameInputAvailableCallback& callback);
  void StartVideo(const media::cast::VideoSenderConfig& config,
                  const FrameInputAvailableCallback& callback);
  void StartUDP(const net::IPEndPoint& local_endpoint,
                const net::IPEndPoint& remote_endpoint);

  void ToggleLogging(bool is_audio, bool enable);
  void GetEventLogsAndReset(bool is_audio, const EventLogsCallback& callback);

 protected:
  // Callback with the result of the initialization.
  // If this callback is called with STATUS_INITIALIZED it will report back
  // to the sinks that it's ready to accept incoming audio / video frames.
  void InitializationResult(media::cast::CastInitializationStatus result) const;

 private:
  // Start encoding threads and initialize the CastEnvironment.
  void Initialize();

  // Configure CastSender. It is ready to accept audio / video frames after
  // receiving a successful call to InitializationResult.
  void StartSendingInternal();

  void StatusNotificationCB(
      media::cast::transport::CastTransportStatus status);

  base::ThreadChecker thread_checker_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  scoped_ptr<media::cast::CastSender> cast_sender_;
  scoped_ptr<media::cast::transport::CastTransportSender> cast_transport_;

  // Configuration for audio and video.
  scoped_ptr<media::cast::AudioSenderConfig> audio_config_;
  scoped_ptr<media::cast::VideoSenderConfig> video_config_;

  FrameInputAvailableCallback audio_frame_input_available_callback_;
  FrameInputAvailableCallback video_frame_input_available_callback_;

  net::IPEndPoint local_endpoint_;
  net::IPEndPoint remote_endpoint_;
  bool transport_configured_;

  scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber_;
  scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber_;

  // Proxy to the IO message loop.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  base::WeakPtrFactory<CastSessionDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
