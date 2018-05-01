// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYNC_READER_H_
#define MEDIA_AUDIO_AUDIO_SYNC_READER_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_output_controller.h"
#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace base {
class SharedMemory;
}

namespace media {

// A AudioOutputController::SyncReader implementation using SyncSocket. This
// is used by AudioOutputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class MEDIA_EXPORT AudioSyncReader : public AudioOutputController::SyncReader {
 public:
  // Create() automatically initializes the AudioSyncReader correctly,
  // and should be strongly preferred over calling the constructor directly!
  AudioSyncReader(
      base::RepeatingCallback<void(const std::string&)> log_callback,
      const AudioParameters& params,
      std::unique_ptr<base::SharedMemory> shared_memory,
      std::unique_ptr<base::CancelableSyncSocket> socket);

  ~AudioSyncReader() override;

  // Returns null on failure.
  static std::unique_ptr<AudioSyncReader> Create(
      base::RepeatingCallback<void(const std::string&)> log_callback,
      const AudioParameters& params,
      base::CancelableSyncSocket* foreign_socket);

  const base::SharedMemory* shared_memory() const {
    return shared_memory_.get();
  }

  void set_max_wait_timeout_for_test(base::TimeDelta time) {
    maximum_wait_time_ = time;
  }

  // AudioOutputController::SyncReader implementations.
  void RequestMoreData(base::TimeDelta delay,
                       base::TimeTicks delay_timestamp,
                       int prior_frames_skipped) override;
  void Read(AudioBus* dest) override;
  void Close() override;

 private:
  // Blocks until data is ready for reading or a timeout expires.  Returns false
  // if an error or timeout occurs.
  bool WaitUntilDataIsReady();

  const base::RepeatingCallback<void(const std::string&)> log_callback_;

  std::unique_ptr<base::SharedMemory> shared_memory_;

  // Mutes all incoming samples. This is used to prevent audible sound
  // during automated testing.
  const bool mute_audio_;

  // Denotes that the most recent socket error has been logged. Used to avoid
  // log spam.
  bool had_socket_error_;

  // Socket for transmitting audio data.
  std::unique_ptr<base::CancelableSyncSocket> socket_;

  const uint32_t output_bus_buffer_size_;

  // Shared memory wrapper used for transferring audio data to Read() callers.
  std::unique_ptr<AudioBus> output_bus_;

  // Track the number of times the renderer missed its real-time deadline and
  // report a UMA stat during destruction.
  size_t renderer_callback_count_;
  size_t renderer_missed_callback_count_;
  size_t trailing_renderer_missed_callback_count_;

  // The maximum amount of time to wait for data from the renderer.  Calculated
  // from the parameters given at construction.
  base::TimeDelta maximum_wait_time_;

  // The index of the audio buffer we're expecting to be sent from the renderer;
  // used to block with timeout for audio data.
  uint32_t buffer_index_;

  DISALLOW_COPY_AND_ASSIGN(AudioSyncReader);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYNC_READER_H_
