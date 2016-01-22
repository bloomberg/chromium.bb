// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_H_

#include "content/browser/gamepad/gamepad_provider.h"
#include "content/common/content_export.h"

namespace content {

// Abstract interface for imlementing platform- (and test-) specific behavior
// for getting the gamepad data.
class CONTENT_EXPORT GamepadDataFetcher {
 public:
  GamepadDataFetcher();
  virtual ~GamepadDataFetcher() {}

  virtual void GetGamepadData(bool devices_changed_hint) = 0;
  virtual void PauseHint(bool paused) {}

  GamepadProvider* provider() { return provider_; }

 protected:
  friend GamepadProvider;

  // To be called by the GamepadProvider on the polling thread;
  void InitializeProvider(GamepadProvider* provider);

  // This call will happen on the gamepad polling thread. Any initialization
  // that needs to happen on that thread should be done here, not in the
  // constructor.
  virtual void OnAddedToProvider() {}

 private:
  GamepadProvider* provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_H_
