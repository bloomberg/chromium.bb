// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NETWORK_PROFILE_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_NETWORK_PROFILE_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Browser;
class FilePath;
class PrefService;

// This class will try to detect if the profile is on a network share and if
// this is the case notify the user with an info bubble.
class NetworkProfileBubble : public views::BubbleDelegateView,
                             public views::ButtonListener,
                             public views::LinkListener {
 public:
  // Verifies that the profile folder is not located on a network share, and if
  // it is shows the warning bubble to the user.
  static void CheckNetworkProfile(const FilePath& profile_path);

  // Returns true if the check for network located profile should be done. This
  // test is only performed up to |kMaxWarnings| times in a row and then
  // repeated after a period of silence that lasts |kSilenceDurationDays| days.
  static bool ShouldCheckNetworkProfile(PrefService* prefs);

  // Shows the notification bubble using the provided |browser|.
  static void ShowNotification(const Browser* browser);

 private:
  explicit NetworkProfileBubble(views::View* anchor);
  virtual ~NetworkProfileBubble();

  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::LinkListener overrides:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // This function creates the notification bubble, attaches it to the
  // |anchor| View and then shows it to the user.
  static void NotifyNetworkProfileDetected();

  // Set to true once the notification check has been performed to avoid showing
  // the notification more than once per browser run.
  // This flag is not thread-safe and should only be accessed on the UI thread!
  static bool notification_shown_;

  DISALLOW_COPY_AND_ASSIGN(NetworkProfileBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NETWORK_PROFILE_BUBBLE_H_
