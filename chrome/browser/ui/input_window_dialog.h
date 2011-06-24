// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_WINDOW_DIALOG_H_
#define CHROME_BROWSER_UI_INPUT_WINDOW_DIALOG_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

// Cross platform access to a modal input window.
class InputWindowDialog {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Checks whether |text| is a valid input string.
    virtual bool IsValid(const string16& text) = 0;

    // Callback for when the user clicks the OK button.
    virtual void InputAccepted(const string16& text) = 0;

    // Callback for when the user clicks the Cancel button.
    virtual void InputCanceled() = 0;
  };

  // Creates a new input window dialog parented to |parent|. Ownership of
  // |delegate| is taken by InputWindowDialog or InputWindowDialog's owner.
  static InputWindowDialog* Create(gfx::NativeWindow parent,
                                   const string16& window_title,
                                   const string16& label,
                                   const string16& contents,
                                   Delegate* delegate);

  // Displays the window.
  virtual void Show() = 0;

  // Closes the window.
  virtual void Close() = 0;

 protected:
  InputWindowDialog() {}
  virtual ~InputWindowDialog() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputWindowDialog);
};

#endif  // CHROME_BROWSER_UI_INPUT_WINDOW_DIALOG_H_
