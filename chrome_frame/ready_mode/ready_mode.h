// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_READY_MODE_H_
#define CHROME_FRAME_READY_MODE_READY_MODE_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>

#include "base/basictypes.h"

interface IWebBrowser2;

// Integrates Ready Mode functionality with a specified IWebBrowser2 instance.
// Displays prompts allowing the user to permanently activate, permanently
// disable, or temporarily disable Chrome Frame whenever a Chrome Frame-enabled
// site is rendered in the browser.
namespace ready_mode {

// Defines an interface for disabling Chrome Frame based on user interaction
// with Ready Mode.
class Delegate {
 public:
  virtual ~Delegate() {}

  // Disables Chrome Frame functionality in the current process. Will be
  // called after the installer has been invoked to manipulate the system or
  // user-level state.
  virtual void DisableChromeFrame() = 0;
};  // class Delegate

// Enables Ready Mode for the specified IWebBrowser2 instance, if Chrome Frame
// is currently in Ready Mode. If Chrome Frame is temporarily or permanently
// declined, will invoke chrome_frame->DisableChromeFrame() to synchronize the
// process state with the system- / user-level state.
void Configure(Delegate* chrome_frame, IWebBrowser2* web_browser);

};  // namespace ready_mode

#endif  // CHROME_FRAME_READY_MODE_READY_MODE_H_
