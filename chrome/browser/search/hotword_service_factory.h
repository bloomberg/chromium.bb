// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class HotwordService;
class Profile;

// Singleton that owns all HotwordServices and associates them with Profiles.
class HotwordServiceFactory : public MediaCaptureDevicesDispatcher::Observer,
                              public BrowserContextKeyedServiceFactory {
 public:
  // Returns the HotwordService for |context|.
  static HotwordService* GetForProfile(content::BrowserContext* context);

  static HotwordServiceFactory* GetInstance();

  // Returns true if the hotwording service is available for |context|.
  static bool IsServiceAvailable(content::BrowserContext* context);

  // Returns true if hotwording is allowed for |context|.
  static bool IsHotwordAllowed(content::BrowserContext* context);

  // Returns the current error message for the service for |context|.
  // A value of 0 indicates no error.
  static int GetCurrentError(content::BrowserContext* context);

  // Returns the current known state of the microphone. Since this state
  // is browser (not profile) specific, it resides in the factory.
  static bool IsMicrophoneAvailable();

  // Overridden from MediaCaptureDevicesDispatcher::Observer
  virtual void OnUpdateAudioDevices(
      const content::MediaStreamDevices& devices) OVERRIDE;

  // This will kick off the monitor that calls OnUpdateAudioDevices when the
  // number of audio devices changes (or is initialized). It needs to be a
  // separate function so it can be called after the service is initialized
  // (i.e., after startup). The monitor can't be initialized during startup
  // because it would slow down startup too much so it is delayed and not
  // called until it's needed by the webui in browser_options_handler.
  void UpdateMicrophoneState();

 private:
  friend struct DefaultSingletonTraits<HotwordServiceFactory>;

  HotwordServiceFactory();
  virtual ~HotwordServiceFactory();

  // Overrides from BrowserContextKeyedServiceFactory:
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  // Must be called from the UI thread since the instance of
  // MediaCaptureDevicesDispatcher can only be accessed on the UI thread.
  void InitializeMicrophoneObserver();

  bool microphone_available() { return microphone_available_; }

  bool microphone_available_;

  DISALLOW_COPY_AND_ASSIGN(HotwordServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_
