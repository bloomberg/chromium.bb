// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BASE_SHELL_DIALOG_H_
#define CHROME_BROWSER_UI_BASE_SHELL_DIALOG_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

// A base class for shell dialogs.
class BaseShellDialog {
 public:
  // Returns true if a shell dialog box is currently being shown modally
  // to the specified owner.
  virtual bool IsRunning(gfx::NativeWindow owning_window) const = 0;

  // Notifies the dialog box that the listener has been destroyed and it should
  // no longer be sent notifications.
  virtual void ListenerDestroyed() = 0;

 protected:
  virtual ~BaseShellDialog() {}
};

#endif  // CHROME_BROWSER_UI_BASE_SHELL_DIALOG_H_
