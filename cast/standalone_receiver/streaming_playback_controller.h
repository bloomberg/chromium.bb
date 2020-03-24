// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_STREAMING_PLAYBACK_CONTROLLER_H_
#define CAST_STANDALONE_RECEIVER_STREAMING_PLAYBACK_CONTROLLER_H_

#include <memory>

#include "cast/streaming/receiver_session.h"
#include "platform/impl/task_runner.h"

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
#include "cast/standalone_receiver/sdl_audio_player.h"
#include "cast/standalone_receiver/sdl_glue.h"
#include "cast/standalone_receiver/sdl_video_player.h"
#else
#include "cast/standalone_receiver/dummy_player.h"
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

namespace openscreen {
namespace cast {

class StreamingPlaybackController final : public ReceiverSession::Client {
 public:
  class Client {
   public:
    virtual void OnPlaybackError(StreamingPlaybackController* controller,
                                 Error error) = 0;
  };

  StreamingPlaybackController(TaskRunner* task_runner,
                              StreamingPlaybackController::Client* client);

  // ReceiverSession::Client overrides.
  void OnNegotiated(const ReceiverSession* session,
                    ReceiverSession::ConfiguredReceivers receivers) override;

  void OnConfiguredReceiversDestroyed(const ReceiverSession* session) override;

  void OnError(const ReceiverSession* session, Error error) override;

 private:
  TaskRunner* const task_runner_;
  StreamingPlaybackController::Client* client_;

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  // NOTE: member ordering is important, since the sub systems must be
  // first-constructed, last-destroyed. Make sure any new SDL related
  // members are added below the sub systems.
  const ScopedSDLSubSystem<SDL_INIT_AUDIO> sdl_audio_sub_system_;
  const ScopedSDLSubSystem<SDL_INIT_VIDEO> sdl_video_sub_system_;
  const SDLEventLoopProcessor sdl_event_loop_;

  SDLWindowUniquePtr window_;
  SDLRendererUniquePtr renderer_;
  std::unique_ptr<SDLAudioPlayer> audio_player_;
  std::unique_ptr<SDLVideoPlayer> video_player_;
#else
  std::unique_ptr<DummyPlayer> audio_player_;
  std::unique_ptr<DummyPlayer> video_player_;
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_STREAMING_PLAYBACK_CONTROLLER_H_
