// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_MANAGER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_MANAGER_H_
#pragma once

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/speech_recognition_event_listener.h"

class Extension;
class Profile;
class SpeechRecognitionTrayIconController;

namespace content {
class NotificationRegistrar;
struct SpeechRecognitionError;
struct SpeechRecognitionResult;
class SpeechRecognizer;
}

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
      content::SpeechRecognitionEventListener* listener,
      net::URLRequestContextGetter* context_getter,
      int session_id,
      const std::string& language,
      const std::string& grammar,
      bool filter_profanities) = 0;

  virtual void StopRecording(bool recognition_failed) = 0;
  virtual bool HasAudioInputDevices() = 0;
  virtual bool IsCapturingAudio() = 0;

  // Called from the UI thread.
  virtual bool HasValidRecognizer() = 0;
};

// Manages the speech input requests and responses from the extensions
// associated to the given profile.
class SpeechInputExtensionManager
    : public base::RefCountedThreadSafe<SpeechInputExtensionManager>,
      public content::SpeechRecognitionEventListener,
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

  typedef base::Callback<void(bool)> IsRecordingCallback;

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
  // This method is expected to be called from the UI thread.
  // The callback will be invoked with the result on this same thread.
  void IsRecording(const IsRecordingCallback& callback);

  // Called by internal ProfileKeyedService class.
  void ShutdownOnUIThread();

  // Methods from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Methods from SpeechRecognitionEventListener.
  virtual void OnRecognitionStart(int session_id) OVERRIDE;
  virtual void OnAudioStart(int session_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE;
  virtual void OnSoundStart(int session_id) OVERRIDE;
  virtual void OnSoundEnd(int session_id) OVERRIDE;
  virtual void OnAudioEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionResult(
      int session_id, const content::SpeechRecognitionResult& result) OVERRIDE;
  virtual void OnRecognitionError(
      int session_id, const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(int session_id, float volume,
                                   float noise_volume) OVERRIDE;
  virtual void OnRecognitionEnd(int session_id) OVERRIDE;

  // Methods for API testing.
  void SetSpeechInputExtensionInterface(
      SpeechInputExtensionInterface* interface);
  SpeechInputExtensionInterface* GetSpeechInputExtensionInterface();

 private:
  // SpeechInputExtensionInterface methods:
  virtual bool IsCapturingAudio() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual bool HasValidRecognizer() OVERRIDE;
  virtual void StartRecording(
      content::SpeechRecognitionEventListener* listener,
      net::URLRequestContextGetter* context_getter,
      int session_id,
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
  void IsRecordingOnIOThread(const IsRecordingCallback& callback);

  void SetRecognitionResultOnUIThread(
      const content::SpeechRecognitionResult& result,
      const std::string& extension_id);
  void DidStartReceivingAudioOnUIThread();
  void StopSucceededOnUIThread();
  void IsRecordingOnUIThread(const IsRecordingCallback& callback, bool result);

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
  scoped_ptr<content::NotificationRegistrar> registrar_;
  SpeechInputExtensionInterface* speech_interface_;
  scoped_refptr<SpeechRecognitionTrayIconController> notification_;

  // Used in the IO thread.
  scoped_refptr<content::SpeechRecognizer> recognizer_;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_INPUT_EXTENSION_MANAGER_H_
