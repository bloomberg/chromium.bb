// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionService;
class HotwordAudioHistoryHandler;
class HotwordClient;
class Profile;

namespace extensions {
class Extension;
}  // namespace extensions

namespace hotword_internal {
// String passed to indicate the training state has changed.
extern const char kHotwordTrainingEnabled[];
}  // namespace hotword_internal

// Provides an interface for the Hotword component that does voice triggered
// search.
class HotwordService : public MediaCaptureDevicesDispatcher::Observer,
                       public extensions::ExtensionRegistryObserver,
                       public KeyedService {
 public:
  // A simple subclass to allow for aborting an install during shutdown.
  // HotwordWebstoreInstaller class is public for testing.
  class HotwordWebstoreInstaller : public extensions::WebstoreStartupInstaller {
   public:
    HotwordWebstoreInstaller(const std::string& webstore_item_id,
                             Profile* profile,
                             const Callback& callback)
        : extensions::WebstoreStartupInstaller(webstore_item_id,
                                               profile,
                                               false,
                                               callback) {}
    void Shutdown();
   protected:
    ~HotwordWebstoreInstaller() override {}
  };

  // Returns true if the hotword supports the current system language.
  static bool DoesHotwordSupportLanguage(Profile* profile);

  // Returns true if hotwording hardware is available.
  static bool IsHotwordHardwareAvailable();

  explicit HotwordService(Profile* profile);
  ~HotwordService() override;

  // Overridden from ExtensionRegisterObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // Overriden from KeyedService
  void Shutdown() override;

  // Checks for whether all the necessary files have downloaded to allow for
  // using the extension.
  virtual bool IsServiceAvailable();

  // Determine if hotwording is allowed in this profile based on field trials
  // and language.
  virtual bool IsHotwordAllowed();

  // Checks if the user has opted into audio logging. Returns true if the user
  // is opted in, false otherwise..
  bool IsOptedIntoAudioLogging();

  // Returns whether always-on hotwording is enabled.
  bool IsAlwaysOnEnabled();

  // Returns whether google.com/NTP/launcher hotwording is enabled.
  bool IsSometimesOnEnabled();

  // Handles enabling/disabling the hotword notification when the user
  // changes the always on search settings.
  void OnHotwordAlwaysOnSearchEnabledChanged(const std::string& pref_name);

  // Called to handle the hotword session from |client|.
  void RequestHotwordSession(HotwordClient* client);
  void StopHotwordSession(HotwordClient* client);
  HotwordClient* client() { return client_; }

  // Checks if the current version of the hotword extension should be
  // uninstalled in order to update to a different language version.
  // Returns true if the extension was uninstalled.
  bool MaybeReinstallHotwordExtension();

  // Checks based on locale if the current version should be uninstalled so that
  // a version with a different language can be installed.
  bool ShouldReinstallHotwordExtension();

  // Helper functions pulled out for testing purposes.
  // UninstallHotwordExtension returns true if the extension was uninstalled.
  virtual bool UninstallHotwordExtension(ExtensionService* extension_service);
  virtual void InstallHotwordExtensionFromWebstore(int num_tries);

  // Sets the pref value of the previous language.
  void SetPreviousLanguagePref();

  // Returns the current error message id. A value of 0 indicates
  // no error.
  int error_message() { return error_message_; }

  bool microphone_available() { return microphone_available_; }

  // These methods are for launching, and getting and setting the launch mode of
  // the Hotword Audio Verification App.
  //
  // OptIntoHotwording first determines if the app needs to be launched, and if
  // so, launches the app (if Audio History is on and a speaker model exists,
  // then we don't need to launch the app).
  //
  // LaunchHotwordAudioVerificationApp launches the app without the above
  // check in the specified |launch_mode|.
  enum LaunchMode {
    HOTWORD_ONLY,
    HOTWORD_AND_AUDIO_HISTORY,
    RETRAIN
  };
  void OptIntoHotwording(const LaunchMode& launch_mode);
  void LaunchHotwordAudioVerificationApp(const LaunchMode& launch_mode);
  virtual LaunchMode GetHotwordAudioVerificationLaunchMode();

  // Called when the SpeakerModelExists request is complete. Either
  // sets the always-on hotword pref to true, or launches the Hotword
  // Audio Verification App, depending on the value of |exists|.
  void SpeakerModelExistsComplete(bool exists);

  // These methods control the speaker training communication between
  // the Hotword Audio Verification App and the Hotword Extension that
  // contains the NaCl module.
  void StartTraining();
  void FinalizeSpeakerModel();
  void StopTraining();
  void NotifyHotwordTriggered();

  // Returns true if speaker training is currently in progress.
  bool IsTraining();

  // Indicate that the currently active user has changed.
  void ActiveUserChanged();

  // Return true if this profile corresponds to the currently active user.
  bool UserIsActive();

  // Returns a pointer to the audio history handler.
  HotwordAudioHistoryHandler* GetAudioHistoryHandler();

  // Sets the audio history handler. Used for tests.
  void SetAudioHistoryHandler(HotwordAudioHistoryHandler* handler);

  // Turn off the currently enabled version of hotwording if one exists.
  void DisableHotwordPreferences();

  // Overridden from MediaCaptureDevicesDispatcher::Observer
  void OnUpdateAudioDevices(
      const content::MediaStreamDevices& devices) override;

 protected:
  // Used in test subclasses.
  scoped_refptr<HotwordWebstoreInstaller> installer_;

 private:
  class HotwordUserSessionStateObserver;

  // Must be called from the UI thread since the instance of
  // MediaCaptureDevicesDispatcher can only be accessed on the UI thread.
  void InitializeMicrophoneObserver();

  // Callback for webstore extension installer.
  void InstalledFromWebstoreCallback(
      int num_tries,
      bool success,
      const std::string& error,
      extensions::webstore_install::Result result);

  // Returns the ID of the extension that may need to be reinstalled.
  std::string ReinstalledExtensionId();

  // Creates a notification for always-on hotwording.
  void ShowHotwordNotification();

  Profile* profile_;

  PrefChangeRegistrar pref_registrar_;

  content::NotificationRegistrar registrar_;

  // For observing the ExtensionRegistry.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  scoped_ptr<HotwordAudioHistoryHandler> audio_history_handler_;

  bool microphone_available_;

  // Indicates if the check for audio devices has been run such that it can be
  // included in the error checking. Audio checking is not done immediately
  // upon start up because of the negative impact on performance.
  bool audio_device_state_updated_;

  HotwordClient* client_;
  int error_message_;
  bool reinstall_pending_;
  // Whether we are currently in the process of training the speaker model.
  bool training_;
  scoped_ptr<HotwordUserSessionStateObserver> session_observer_;

  // Stores the launch mode for the Hotword Audio Verification App.
  LaunchMode hotword_audio_verification_launch_mode_;

  // The WeakPtrFactory should be the last member, so the weak pointer
  // gets invalidated before the destructors for other members run,
  // to avoid callbacks into a half-destroyed object.
  base::WeakPtrFactory<HotwordService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HotwordService);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
