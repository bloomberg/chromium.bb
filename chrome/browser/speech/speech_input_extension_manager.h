// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_MANAGER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_MANAGER_H_
#pragma once

#include "base/synchronization/lock.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include <string>

class Extension;
class Profile;
class SpeechInputExtensionNotification;

namespace net {
class URLRequestContextGetter;
}

// Used for API tests.
class SpeechInputExtensionInterface {
 public:
  SpeechInputExtensionInterface();
  virtual ~SpeechInputExtensionInterface();

  // Called from the IO thread.
  virtual void StartRecording(
      speech_input::SpeechRecognizerDelegate* delegate,
      net::URLRequestContextGetter* context_getter,
      int caller_id,
      const std::string& language,
      const std::string& grammar,
      bool filter_profanities) = 0;

  virtual void StopRecording(bool recognition_failed) = 0;
  virtual bool HasAudioInputDevices() = 0;

  // Called from the UI thread.
  virtual bool HasValidRecognizer() = 0;

  // Called from both IO and UI threads.
  virtual bool IsRecordingInProcess() = 0;

 protected:
  scoped_refptr<speech_input::SpeechRecognizer> recognizer_;
};

// Manages the speech input requests and responses from the extensions
// associated to the given profile.
class SpeechInputExtensionManager
    : public base::RefCountedThreadSafe<SpeechInputExtensionManager>,
      public speech_input::SpeechRecognizerDelegate,
      public content::NotificationObserver,
      private SpeechInputExtensionInterface {
 public:
  enum State {
    kIdle = 0,
    kStarting,
    kRecording,
    kStopping,
    kShutdown // Internal sink state when the profile is destroyed on shutdown.
  };

  // Structure containing the details of the speech input failed notification.
  struct ExtensionError {
    std::string extension_id_;
    std::string error_;

    ExtensionError(const std::string& extension_id, const std::string& error)
        : extension_id_(extension_id), error_(error) {}
  };

  // Should not be used directly. Managed by a ProfileKeyedServiceFactory.
  explicit SpeechInputExtensionManager(Profile* profile);

  // Returns the corresponding manager for the given profile, creating
  // a new one if required.
  static SpeechInputExtensionManager* GetForProfile(Profile* profile);

  // Initialize the ProfileKeyedServiceFactory.
  static void InitializeFactory();

  // Request to start speech recognition for the provided extension.
  bool Start(const std::string& extension_id,
             const std::string& language,
             const std::string& grammar,
             bool filter_profanities,
             std::string* error);

  // Request to stop an ongoing speech recognition.
  bool Stop(const std::string& extension_id, std::string* error);

  // Retrieve the actual state of the API manager.
  State state() const { return state_; }

  // Check if recording is currently ongoing in Chrome.
  bool IsRecording();

  // Called by internal ProfileKeyedService class.
  void ShutdownOnUIThread();

  // Methods from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Methods from SpeechRecognizerDelegate.
  virtual void SetRecognitionResult(
      int caller_id,
      const content::SpeechInputResult& result) OVERRIDE;

  virtual void DidStartReceivingAudio(int caller_id) OVERRIDE;
  virtual void DidCompleteRecording(int caller_id) OVERRIDE;
  virtual void DidCompleteRecognition(int caller_id) OVERRIDE;
  virtual void DidStartReceivingSpeech(int caller_id) OVERRIDE;
  virtual void DidStopReceivingSpeech(int caller_id) OVERRIDE;
  virtual void OnRecognizerError(int caller_id,
                                 content::SpeechInputError error)
                                 OVERRIDE;
  virtual void DidCompleteEnvironmentEstimation(int caller_id) OVERRIDE;
  virtual void SetInputVolume(int caller_id, float volume,
                              float noise_volume) OVERRIDE;

  // Methods for API testing.
  void SetSpeechInputExtensionInterface(
      SpeechInputExtensionInterface* interface);
  SpeechInputExtensionInterface* GetSpeechInputExtensionInterface();

 private:
  // SpeechInputExtensionInterface methods:
  virtual bool IsRecordingInProcess() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual bool HasValidRecognizer() OVERRIDE;

  virtual void StartRecording(
      speech_input::SpeechRecognizerDelegate* delegate,
      net::URLRequestContextGetter* context_getter,
      int caller_id,
      const std::string& language,
      const std::string& grammar,
      bool filter_profanities) OVERRIDE;

  virtual void StopRecording(bool recognition_failed) OVERRIDE;

  // Internal methods.
  void StartOnIOThread(
      net::URLRequestContextGetter* context_getter,
      const std::string& language,
      const std::string& grammar,
      bool filter_profanities);
  void ForceStopOnIOThread();

  void SetRecognitionResultOnUIThread(
      const content::SpeechInputResult& result,
      const std::string& extension_id);
  void DidStartReceivingAudioOnUIThread();
  void StopSucceededOnUIThread();

  void DispatchError(const std::string& error, bool dispatch_event);
  void DispatchEventToExtension(const std::string& extension_id,
                                const std::string& event,
                                const std::string& json_args);
  void ExtensionUnloaded(const std::string& extension_id);

  void SetInputVolumeOnUIThread(float volume);
  void ResetToIdleState();

  virtual ~SpeechInputExtensionManager();

  friend class base::RefCountedThreadSafe<SpeechInputExtensionManager>;
  class Factory;

  // Lock used to allow exclusive access to the state variable and methods that
  // either read or write on it. This is required since the speech code
  // operates in the IO thread while the extension code uses the UI thread.
  base::Lock state_lock_;

  // Used in the UI thread but also its raw value as notification
  // source in the IO thread, guarded by the state lock and value.
  Profile* profile_;

  // Used in both threads, guarded by the state lock.
  State state_;
  std::string extension_id_in_use_;

  // Used in the UI thread.
  content::NotificationRegistrar registrar_;
  SpeechInputExtensionInterface* speech_interface_;
  scoped_ptr<SpeechInputExtensionNotification> notification_;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_MANAGER_H_
