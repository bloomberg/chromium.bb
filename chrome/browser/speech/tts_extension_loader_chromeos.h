// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_EXTENSION_LOADER_CHROMEOS_H_
#define CHROME_BROWSER_SPEECH_TTS_EXTENSION_LOADER_CHROMEOS_H_

#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "extensions/browser/event_router.h"

class Profile;

// Profile-keyed class that loads a built-in TTS component extension
// into a given profile on Chrome OS.
class TtsExtensionLoaderChromeOs
    : public BrowserContextKeyedService,
      public extensions::EventRouter::Observer {
 public:
  static TtsExtensionLoaderChromeOs* GetInstance(Profile* profile);

  // Returns true if the extension was not previously loaded and is now
  // loading. This class will call
  // ExtensionTtsController::RetrySpeakingQueuedUtterances when the
  // extension finishes loading.
  bool LoadTtsExtension();

  // Implementation of BrowserContextKeyedService.
  virtual void Shutdown() OVERRIDE;

  // Implementation of extensions::EventRouter::Observer.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  // The state of TTS for this profile.
  enum TtsState {
    TTS_NOT_LOADED,
    TTS_LOAD_REQUESTED,
    TTS_LOADING,
    TTS_LOADED
  };

  explicit TtsExtensionLoaderChromeOs(Profile* profile);
  virtual ~TtsExtensionLoaderChromeOs() {}

  bool IsTtsLoadedInThisProfile();

  Profile* profile_;
  TtsState tts_state_;

  friend class TtsExtensionLoaderChromeOsFactory;

  DISALLOW_COPY_AND_ASSIGN(TtsExtensionLoaderChromeOs);
};

#endif  // CHROME_BROWSER_SPEECH_TTS_EXTENSION_LOADER_CHROMEOS_H_
