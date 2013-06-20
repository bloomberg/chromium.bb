// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_AUDIO_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_AUDIO_CONTROLLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_piece.h"
#include "media/audio/audio_parameters.h"

namespace base {
class MessageLoopProxy;
}

class Profile;

// This class controls the sound playing for notifications. Note that this class
// belongs to the audio thread and self-owned.
class NotificationAudioController {
 public:
  NotificationAudioController();

  // Requests the audio thread to play a sound for a notification. |data|
  // specifies the raw audio data in wav file format. |notification_id|
  // and |profile| represents the id of the notification for the sound.
  // When this method is called during playing sound for the specified
  // |notification_id|, it will stop playing the sound and start the sound for
  // the newly specified |data|.
  void RequestPlayNotificationSound(
      const std::string& notification_id,
      const Profile* profile,
      const base::StringPiece& data);

  // Request shutdown process in the audio thread. Delete itself when all
  // processes have finished.
  void RequestShutdown();

 private:
  class AudioHandler;
  friend class NotificationAudioControllerTest;

  ~NotificationAudioController();

  void UseFakeAudioOutputForTest();

  // Request the current number of audio handlers to the audio thread. |on_get|
  // will be called with the result when finished.
  void GetAudioHandlersSizeForTest(const base::Callback<void(size_t)>& on_get);

  // Actually start playing the notification sound in the audio thread.
  void PlayNotificationSound(
      const std::string& notification_id,
      const Profile* profile,
      const base::StringPiece& data);

  // Removes all instances in |audio_handlers_|.
  void Shutdown();

  // Gets the current size of |audio_handlers_| in the audio thread.
  size_t GetAudioHandlersSizeCallback();

  // Called when the sound for |audio_handler| has finished.
  void OnNotificationSoundFinished(AudioHandler* audio_handler);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ScopedVector<AudioHandler> audio_handlers_;

  media::AudioParameters::Format output_format_;

  DISALLOW_COPY_AND_ASSIGN(NotificationAudioController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_AUDIO_CONTROLLER_H_
