// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_ACTOR_H_
#pragma once

namespace chromeos {

class HelpAppLauncher;
class NetworkSelectionView;

// Interface for dependency injection between NetworkScreen and its actual
// representation, either views based or WebUI. Owned by NetworkScreen.
class NetworkScreenActor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnContinuePressed() = 0;
  };

  virtual ~NetworkScreenActor() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Returns the size of the screen contents.
  virtual gfx::Size GetScreenSize() const = 0;

  // Shows error message in a bubble.
  virtual void ShowError(const string16& message) = 0;

  // Hides error messages showing no error state.
  virtual void ClearErrors() = 0;

  // Shows network connecting status or network selection otherwise.
  virtual void ShowConnectingStatus(
      bool connecting,
      const string16& network_id) = 0;

  // Sets whether continue control is enabled.
  virtual void EnableContinue(bool enabled) = 0;

  // Returns if continue control is enabled.
  virtual bool IsContinueEnabled() const = 0;

  // Returns true if we're in the connecting state.
  virtual bool IsConnecting() const = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_ACTOR_H_
