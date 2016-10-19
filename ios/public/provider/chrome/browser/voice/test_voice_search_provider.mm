// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/voice/test_voice_search_provider.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_language.h"

TestVoiceSearchProvider::TestVoiceSearchProvider() {}

TestVoiceSearchProvider::~TestVoiceSearchProvider() {}

NSArray* TestVoiceSearchProvider::GetAvailableLanguages() const {
  base::scoped_nsobject<VoiceSearchLanguage> en([[VoiceSearchLanguage alloc]
          initWithIdentifier:@"en-US"
                 displayName:@"English (US)"
      localizationPreference:nil]);
  return @[ en ];
}

AudioSessionController* TestVoiceSearchProvider::GetAudioSessionController()
    const {
  // TODO(rohitrao): Return a TestAudioSessionController once the class with the
  // same name is removed from the internal tree.
  return nullptr;
}
