// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_debug_recordings_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_log_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/audio/audio_manager.h"

using content::BrowserThread;

// Keys used to attach handler to the RenderProcessHost
const char AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey[] =
    "kAudioDebugRecordingsHandlerKey";

namespace {

// Returns a path name to be used as prefix for audio debug recordings files.
base::FilePath GetAudioDebugRecordingsPrefixPath(
    const base::FilePath& directory,
    uint64_t audio_debug_recordings_id) {
  static const char kAudioDebugRecordingsFilePrefix[] = "AudioDebugRecordings.";
  return directory.AppendASCII(kAudioDebugRecordingsFilePrefix +
                               base::Int64ToString(audio_debug_recordings_id));
}

base::FilePath GetLogDirectoryAndEnsureExists(Profile* profile) {
  base::FilePath log_dir_path =
      WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile->GetPath());
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(log_dir_path, &error)) {
    DLOG(ERROR) << "Could not create WebRTC log directory, error: " << error;
    return base::FilePath();
  }
  return log_dir_path;
}

}  // namespace

AudioDebugRecordingsHandler::AudioDebugRecordingsHandler(
    Profile* profile,
    media::AudioManager* audio_manager)
    : profile_(profile),
      is_audio_debug_recordings_in_progress_(false),
      current_audio_debug_recordings_id_(0),
      audio_manager_(audio_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(audio_manager_);
}

AudioDebugRecordingsHandler::~AudioDebugRecordingsHandler() {}

void AudioDebugRecordingsHandler::StartAudioDebugRecordings(
    content::RenderProcessHost* host,
    base::TimeDelta delay,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&GetLogDirectoryAndEnsureExists, profile_),
      base::Bind(&AudioDebugRecordingsHandler::DoStartAudioDebugRecordings,
                 this, host, delay, callback, error_callback));
}

void AudioDebugRecordingsHandler::StopAudioDebugRecordings(
    content::RenderProcessHost* host,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool is_manual_stop = true;
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&GetLogDirectoryAndEnsureExists, profile_),
      base::Bind(&AudioDebugRecordingsHandler::DoStopAudioDebugRecordings, this,
                 host, is_manual_stop, current_audio_debug_recordings_id_,
                 callback, error_callback));
}

void AudioDebugRecordingsHandler::DoStartAudioDebugRecordings(
    content::RenderProcessHost* host,
    base::TimeDelta delay,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (is_audio_debug_recordings_in_progress_) {
    error_callback.Run("Audio debug recordings already in progress");
    return;
  }

  is_audio_debug_recordings_in_progress_ = true;
  base::FilePath prefix_path = GetAudioDebugRecordingsPrefixPath(
      log_directory, ++current_audio_debug_recordings_id_);
  host->EnableAudioDebugRecordings(prefix_path);

  // AudioManager is deleted on the audio thread, and the AudioManager outlives
  // this object, which is owned by content::RenderProcessHost, so it's safe to
  // post unretained.
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&media::AudioManager::EnableOutputDebugRecording,
                     base::Unretained(audio_manager_), prefix_path));

  if (delay.is_zero()) {
    const bool is_stopped = false, is_manual_stop = false;
    callback.Run(prefix_path.AsUTF8Unsafe(), is_stopped, is_manual_stop);
    return;
  }

  const bool is_manual_stop = false;
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&AudioDebugRecordingsHandler::DoStopAudioDebugRecordings,
                     this, host, is_manual_stop,
                     current_audio_debug_recordings_id_, callback,
                     error_callback, log_directory),
      delay);
}

void AudioDebugRecordingsHandler::DoStopAudioDebugRecordings(
    content::RenderProcessHost* host,
    bool is_manual_stop,
    uint64_t audio_debug_recordings_id,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_LE(audio_debug_recordings_id, current_audio_debug_recordings_id_);

  base::FilePath prefix_path = GetAudioDebugRecordingsPrefixPath(
      log_directory, audio_debug_recordings_id);
  // Prevent an old posted StopAudioDebugRecordings() call to stop a newer dump.
  // This could happen in a sequence like:
  //   Start(10);  // Start dump 1. Post Stop() to run after 10 seconds.
  //   Stop();     // Manually stop dump 1 before 10 seconds;
  //   Start(20);  // Start dump 2. Posted Stop() for 1 should not stop dump 2.
  if (audio_debug_recordings_id < current_audio_debug_recordings_id_) {
    const bool is_stopped = false;
    callback.Run(prefix_path.AsUTF8Unsafe(), is_stopped, is_manual_stop);
    return;
  }

  if (!is_audio_debug_recordings_in_progress_) {
    error_callback.Run("No audio debug recording in progress");
    return;
  }

  // AudioManager is deleted on the audio thread, and the AudioManager outlives
  // this object, which is owned by content::RenderProcessHost, so it's safe to
  // post unretained.
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&media::AudioManager::DisableOutputDebugRecording,
                     base::Unretained(audio_manager_)));

  host->DisableAudioDebugRecordings();

  is_audio_debug_recordings_in_progress_ = false;
  const bool is_stopped = true;
  callback.Run(prefix_path.AsUTF8Unsafe(), is_stopped, is_manual_stop);
}
