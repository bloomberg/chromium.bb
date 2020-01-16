// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/streaming_playback_controller.h"

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
#include "cast/standalone_receiver/sdl_audio_player.h"
#include "cast/standalone_receiver/sdl_glue.h"
#include "cast/standalone_receiver/sdl_video_player.h"
#else
#include "cast/standalone_receiver/dummy_player.h"
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

namespace openscreen {
namespace cast {

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
StreamingPlaybackController::StreamingPlaybackController(
    TaskRunnerImpl* task_runner)
    : task_runner_(task_runner),
      sdl_event_loop_(task_runner_, [&] { task_runner_->RequestStopSoon(); }) {
  constexpr int kDefaultWindowWidth = 1280;
  constexpr int kDefaultWindowHeight = 720;
  window_ = MakeUniqueSDLWindow(
      "Cast Streaming Receiver Demo",
      SDL_WINDOWPOS_UNDEFINED /* initial X position */,
      SDL_WINDOWPOS_UNDEFINED /* initial Y position */, kDefaultWindowWidth,
      kDefaultWindowHeight, SDL_WINDOW_RESIZABLE);
  OSP_CHECK(window_) << "Failed to create SDL window: " << SDL_GetError();
  renderer_ = MakeUniqueSDLRenderer(window_.get(), -1, 0);
  OSP_CHECK(renderer_) << "Failed to create SDL renderer: " << SDL_GetError();
}
#else
StreamingPlaybackController::StreamingPlaybackController(
    TaskRunnerImpl* task_runner)
    : task_runner_(task_runner) {
  OSP_DCHECK(task_runner_ != nullptr);
}
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

void StreamingPlaybackController::OnNegotiated(
    const ReceiverSession* session,
    ReceiverSession::ConfiguredReceivers receivers) {
#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  if (receivers.audio) {
    audio_player_ = std::make_unique<SDLAudioPlayer>(
        &Clock::now, task_runner_, receivers.audio.value().receiver, [&] {
          OSP_LOG_ERROR << audio_player_->error_status().message();
          task_runner_->RequestStopSoon();
        });
  }
  if (receivers.video) {
    video_player_ = std::make_unique<SDLVideoPlayer>(
        &Clock::now, task_runner_, receivers.video.value().receiver,
        renderer_.get(), [&] {
          OSP_LOG_ERROR << video_player_->error_status().message();
          task_runner_->RequestStopSoon();
        });
  }
#else
  if (receivers.audio) {
    audio_player_ =
        std::make_unique<DummyPlayer>(receivers.audio.value().receiver);
  }

  if (receivers.video) {
    video_player_ =
        std::make_unique<DummyPlayer>(receivers.video.value().receiver);
  }
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
}

void StreamingPlaybackController::OnReceiversDestroyed(
    const ReceiverSession* session) {
  audio_player_.reset();
  video_player_.reset();
}

void StreamingPlaybackController::OnError(const ReceiverSession* session,
                                          Error error) {
  OSP_LOG_FATAL << "Failure reported to demo client for session " << session
                << ": " << error;
}

}  // namespace cast
}  // namespace openscreen
