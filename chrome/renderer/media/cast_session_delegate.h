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
#include "media/cast/logging/logging_defines.h"

namespace base {
class BinaryValue;
class DictionaryValue;
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
  typedef base::Callback<void(const scoped_refptr<
      media::cast::AudioFrameInput>&)> AudioFrameInputAvailableCallback;
  typedef base::Callback<void(const scoped_refptr<
      media::cast::VideoFrameInput>&)> VideoFrameInputAvailableCallback;
  typedef base::Callback<void(scoped_ptr<base::BinaryValue>)> EventLogsCallback;
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)> StatsCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  CastSessionDelegate();
  virtual ~CastSessionDelegate();

  void StartAudio(const media::cast::AudioSenderConfig& config,
                  const AudioFrameInputAvailableCallback& callback,
                  const ErrorCallback& error_callback);

  void StartVideo(const media::cast::VideoSenderConfig& config,
                  const VideoFrameInputAvailableCallback& callback,
                  const ErrorCallback& error_callback);

  void StartUDP(const net::IPEndPoint& local_endpoint,
                const net::IPEndPoint& remote_endpoint);

  void ToggleLogging(bool is_audio, bool enable);
  void GetEventLogsAndReset(bool is_audio, const EventLogsCallback& callback);
  void GetStatsAndReset(bool is_audio, const StatsCallback& callback);

 protected:
  // Callback with the result of the initialization.
  // If this callback is called with STATUS_INITIALIZED it will report back
  // to the sinks that it's ready to accept incoming audio / video frames.
  void InitializationResultCB(
      media::cast::CastInitializationStatus result) const;

 private:
  void StatusNotificationCB(
      media::cast::transport::CastTransportStatus status);

  // Adds logs collected from transport on browser side.
  void LogRawEvents(const std::vector<media::cast::PacketEvent>& packet_events);

  base::ThreadChecker thread_checker_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  scoped_ptr<media::cast::CastSender> cast_sender_;
  scoped_ptr<media::cast::transport::CastTransportSender> cast_transport_;

  AudioFrameInputAvailableCallback audio_frame_input_available_callback_;
  VideoFrameInputAvailableCallback video_frame_input_available_callback_;

  scoped_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber_;
  scoped_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber_;

  // Proxy to the IO message loop.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  base::WeakPtrFactory<CastSessionDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
