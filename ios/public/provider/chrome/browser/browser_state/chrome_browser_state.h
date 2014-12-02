// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ios/web/public/browser_state.h"

class PrefService;

namespace ios {

// This class is a Chrome-specific extension of the BrowserState interface.
class ChromeBrowserState : public web::BrowserState {
 public:
  ~ChromeBrowserState() override {}

  // Returns the ChromeBrowserState corresponding to the given BrowserState.
  static ChromeBrowserState* FromBrowserState(BrowserState* browser_state);

  // Returns the original "recording" ChromeBrowserState. This method returns
  // |this| if the ChromeBrowserState is not incognito.
  virtual ChromeBrowserState* GetOriginalChromeBrowserState() = 0;

  // Returns the incognito version of this ChromeBrowserState. The returned
  // ChromeBrowserState instance is owned by this ChromeBrowserState instance.
  // WARNING: This will create the OffTheRecord ChromeBrowserState if it
  // doesn't already exist.
  virtual ChromeBrowserState* GetOffTheRecordChromeBrowserState() = 0;

  // Destroys the OffTheRecord ChromeBrowserState that is associated with this
  // ChromeBrowserState, if one exists.
  virtual void DestroyOffTheRecordChromeBrowserState() = 0;

  // Retrieves a pointer to the PrefService that manages the preferences.
  virtual PrefService* GetPrefs() = 0;

 protected:
  ChromeBrowserState() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserState);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_H_
