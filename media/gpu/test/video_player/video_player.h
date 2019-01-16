// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace media {
namespace test {

class FrameRenderer;
class Video;
class VideoDecoderClient;
class VideoFrameValidator;

// Default timeout used when waiting for events.
constexpr base::TimeDelta kDefaultTimeout = base::TimeDelta::FromSeconds(10);

enum class VideoPlayerState : size_t {
  kUninitialized = 0,
  kIdle,
  kDecoding,
  kDestroyed,
};

enum class VideoPlayerEvent : size_t {
  kFrameDecoded,
  kFlushing,
  kFlushDone,
  kResetting,
  kResetDone,
  kNumEvents,
};

// The video player provides a framework to build video decode accelerator tests
// upon. It provides methods to manipulate video playback, and wait for specific
// events to occur.
class VideoPlayer {
 public:
  using EventCallback = base::RepeatingCallback<void(VideoPlayerEvent)>;

  ~VideoPlayer();

  // Create an instance of the video player. The |frame_renderer| and
  // |frame_validator| will not be owned by the video player. The caller should
  // guarantee they exists for the entire lifetime of the video player.
  static std::unique_ptr<VideoPlayer> Create(
      FrameRenderer* frame_renderer,
      VideoFrameValidator* frame_validator);

  // Set the video stream to be played. The |video| will not be owned by the
  // video player. A decoder will be set up for the specified video stream.
  void SetStream(const Video* video);

  void Play();
  void Stop();
  void Reset();
  void Flush();

  // Get current media time.
  base::TimeDelta GetCurrentTime() const;
  // Get the current frame number.
  size_t GetCurrentFrame() const;
  // Get the current state of the video player.
  VideoPlayerState GetState() const;

  // Wait for an event to occur the specified number of times. All events that
  // occurred since last calling this function will be taken into account. All
  // events with different types will be consumed. Will return false if the
  // specified timeout is exceeded while waiting for the events.
  bool WaitForEvent(VideoPlayerEvent event,
                    size_t times = 1,
                    base::TimeDelta max_wait = kDefaultTimeout);

  // Get the number of times the specified event occurred.
  size_t GetEventCount(VideoPlayerEvent event) const;

 private:
  VideoPlayer();

  void Initialize(FrameRenderer* frame_renderer,
                  VideoFrameValidator* frame_validator);
  void Destroy();

  // Notify the client an event has occurred (e.g. frame decoded).
  void NotifyEvent(VideoPlayerEvent event);

  const Video* video_;
  VideoPlayerState video_player_state_;
  std::unique_ptr<VideoDecoderClient> decoder_client_;

  mutable base::Lock event_lock_;
  base::ConditionVariable event_cv_;

  std::vector<VideoPlayerEvent> video_player_events_ GUARDED_BY(event_lock_);
  size_t video_player_event_counts_[static_cast<size_t>(
      VideoPlayerEvent::kNumEvents)] GUARDED_BY(event_lock_);
  // The next event ID to start at, when waiting for events.
  size_t event_id_ GUARDED_BY(event_lock_);

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(VideoPlayer);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_H_
