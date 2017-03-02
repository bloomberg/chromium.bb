// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_DEBUG_RECORDINGS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_AUDIO_DEBUG_RECORDINGS_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace content {
class RenderProcessHost;
}

namespace media {
class AudioManager;
}

class Profile;

// AudioDebugRecordingsHandler provides an interface to start and stop
// AudioDebugRecordings, including WebRTC AEC dumps. Lives on the UI thread.
class AudioDebugRecordingsHandler
    : public base::RefCountedThreadSafe<AudioDebugRecordingsHandler> {
 public:
  typedef base::Callback<void(bool, const std::string&)> GenericDoneCallback;
  typedef base::Callback<void(const std::string&)> RecordingErrorCallback;
  typedef base::Callback<void(const std::string&, bool, bool)>
      RecordingDoneCallback;

  // Key used to attach the handler to the RenderProcessHost
  static const char kAudioDebugRecordingsHandlerKey[];

  AudioDebugRecordingsHandler(Profile* profile,
                              media::AudioManager* audio_manager);

  // Starts an audio debug recording. The recording lasts the given |delay|,
  // unless |delay| is zero, in which case recording will continue until
  // StopAudioDebugRecordings() is explicitly invoked.
  // |callback| is invoked once recording stops. If |delay| is zero
  // |callback| is invoked once recording starts.
  // If a recording was already in progress, |error_callback| is invoked instead
  // of |callback|.
  void StartAudioDebugRecordings(content::RenderProcessHost* host,
                                 base::TimeDelta delay,
                                 const RecordingDoneCallback& callback,
                                 const RecordingErrorCallback& error_callback);

  // Stops an audio debug recording. |callback| is invoked once recording
  // stops. If no recording was in progress, |error_callback| is invoked instead
  // of |callback|.
  void StopAudioDebugRecordings(content::RenderProcessHost* host,
                                const RecordingDoneCallback& callback,
                                const RecordingErrorCallback& error_callback);

 private:
  friend class base::RefCountedThreadSafe<AudioDebugRecordingsHandler>;

  virtual ~AudioDebugRecordingsHandler();

  base::FilePath GetLogDirectoryAndEnsureExists();

  // Helper for starting audio debug recordings.
  void DoStartAudioDebugRecordings(content::RenderProcessHost* host,
                                   base::TimeDelta delay,
                                   const RecordingDoneCallback& callback,
                                   const RecordingErrorCallback& error_callback,
                                   const base::FilePath& log_directory);

  // Helper for stopping audio debug recordings.
  void DoStopAudioDebugRecordings(content::RenderProcessHost* host,
                                  bool is_manual_stop,
                                  uint64_t audio_debug_recordings_id,
                                  const RecordingDoneCallback& callback,
                                  const RecordingErrorCallback& error_callback,
                                  const base::FilePath& log_directory);

  // The profile associated with our renderer process.
  Profile* const profile_;

  // Set if recordings are in progress.
  bool is_audio_debug_recordings_in_progress_;

  // This counter allows saving each debug recording in separate files.
  uint64_t current_audio_debug_recordings_id_;

  // Audio manager, used for enabling output recordings.
  media::AudioManager* const audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingsHandler);
};

#endif  // CHROME_BROWSER_MEDIA_AUDIO_DEBUG_RECORDINGS_HANDLER_H_
