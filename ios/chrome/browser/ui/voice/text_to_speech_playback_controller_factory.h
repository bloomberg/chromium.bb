// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class TextToSpeechPlaybackController;

// TextToSpeechPlaybackControllerFactory attaches
// TextToSpeechPlaybackControllers to ChromeBrowserStates.
class TextToSpeechPlaybackControllerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // TextToSpeechPlaybackController.
  static TextToSpeechPlaybackController* GetForBrowserState(
      ChromeBrowserState* browser_state);
  // Getter for singleton instance.
  static TextToSpeechPlaybackControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<TextToSpeechPlaybackControllerFactory>;

  TextToSpeechPlaybackControllerFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(TextToSpeechPlaybackControllerFactory);
};

#endif  // IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
