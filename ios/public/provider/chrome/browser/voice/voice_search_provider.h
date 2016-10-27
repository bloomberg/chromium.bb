// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_PROVIDER_H_

#include <Foundation/Foundation.h>
#include <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class AudioSessionController;
@protocol VoiceSearchBar;
class VoiceSearchController;

namespace ios {
class ChromeBrowserState;
}

// VoiceSearchProvider allows embedders to provide functionality related to
// voice search.
class VoiceSearchProvider {
 public:
  VoiceSearchProvider() = default;
  virtual ~VoiceSearchProvider() = default;

  // Returns the list of available voice search languages.
  virtual NSArray* GetAvailableLanguages() const;

  // Returns the singleton audio session controller.
  virtual AudioSessionController* GetAudioSessionController() const;

  // Creates a new VoiceSearchController object.
  virtual scoped_refptr<VoiceSearchController> CreateVoiceSearchController(
      ios::ChromeBrowserState* browser_state) const;

  // Creates a new VoiceSearchBar.  The caller assumes ownership.
  virtual UIView<VoiceSearchBar>* CreateVoiceSearchBar(CGRect frame) const
      NS_RETURNS_RETAINED;

 private:
  DISALLOW_COPY_AND_ASSIGN(VoiceSearchProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_PROVIDER_H_
