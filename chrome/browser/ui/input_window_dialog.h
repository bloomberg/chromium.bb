// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_WINDOW_DIALOG_H_
#define CHROME_BROWSER_UI_INPUT_WINDOW_DIALOG_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

// Cross platform access to a modal input window.
class InputWindowDialog {
 public:
  enum ButtonType {
    BUTTON_TYPE_ADD,
    BUTTON_TYPE_SAVE,
  };
  typedef std::vector<std::pair<string16, string16> > LabelContentsPairs;
  typedef std::vector<string16> InputTexts;

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Checks whether |text| is a valid input string.
    virtual bool IsValid(const InputTexts& texts) = 0;

    // Callback for when the user clicks the OK button.
    virtual void InputAccepted(const InputTexts& texts) = 0;

    // Callback for when the user clicks the Cancel button.
    virtual void InputCanceled() = 0;
  };

  // Creates a new input window dialog parented to |parent|. Ownership of
  // |delegate| is taken by InputWindowDialog or InputWindowDialog's owner.
  static InputWindowDialog* Create(
      gfx::NativeWindow parent,
      const string16& window_title,
      const LabelContentsPairs& label_contents_pairs,
      Delegate* delegate,
      ButtonType type);

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
