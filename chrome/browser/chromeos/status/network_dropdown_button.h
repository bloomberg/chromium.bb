// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
#pragma once

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/views/dropdown_button.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"

namespace chromeos {

// The network dropdown button with menu. Used on welcome screen.
// This class will handle getting the networks to show connected network
// at top level and populating the menu.
// See NetworkMenu for more details.
class NetworkDropdownButton : public DropDownButton,
                              public NetworkMenu,
                              public NetworkLibrary::NetworkManagerObserver {
 public:
  NetworkDropdownButton(bool browser_mode, gfx::NativeWindow parent_window);
  virtual ~NetworkDropdownButton();

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation);

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj);

  // Refreshes button state. Used when language has been changed.
  void Refresh();

 protected:
  // NetworkMenu implementation:
  virtual bool IsBrowserMode() const { return browser_mode_; }
  virtual gfx::NativeWindow GetNativeWindow() const { return parent_window_; }
  virtual void OpenButtonOptions() {}
  virtual bool ShouldOpenButtonOptions() const { return false; }

 private:
  bool browser_mode_;

  // The throb animation that does the wifi connecting animation.
  ui::ThrobAnimation animation_connecting_;

  // The duration of the icon throbbing in milliseconds.
  static const int kThrobDuration;

  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdownButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
