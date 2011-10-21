// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONFIRM_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_CONFIRM_BUBBLE_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Image;
class Point;
}

// An interface implemented by objects wishing to control an ConfirmBubbleView.
// To use this class to implement a bubble menu, we need two steps:
// 1. Implement a class derived from this class.
// 2. Call ConfirmBubbleModel::Show() with the class implemented in 1.
class ConfirmBubbleModel {
 public:
  enum BubbleButton {
    BUTTON_NONE   = 0,
    BUTTON_OK     = 1 << 0,
    BUTTON_CANCEL = 1 << 1,
  };

  ConfirmBubbleModel();
  virtual ~ConfirmBubbleModel();

  // Returns the title string and the message string to be displayed for this
  // bubble menu. These must not be empty strings.
  virtual string16 GetTitle() const = 0;
  virtual string16 GetMessageText() const = 0;

  // Returns an icon for the bubble. This image should be owned by the
  // ResourceBundle and callers should not take ownership of it. Must not return
  // NULL.
  virtual gfx::Image* GetIcon() const = 0;

  // Return the buttons to be shown for this bubble menu. This function returns
  // a combination of BubbleButton values, e.g. when we show both an OK button
  // and a cancel button, it should return (BUTTON_OK | BUTTON_CANCEL). (This is
  // the default implementation.)
  virtual int GetButtons() const;

  // Return the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual string16 GetButtonLabel(BubbleButton button) const;

  // Called when the OK button is pressed.
  virtual void Accept();

  // Called when the Cancel button is pressed.
  virtual void Cancel();

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // and empty string.
  virtual string16 GetLinkText() const;

  // Called when the Link is clicked.
  virtual void LinkClicked();

  // Creates a bubble and shows it with its top center at the specified
  // |origin|. A bubble created by this function takes ownership of the
  // specified |model|.
  static void Show(gfx::NativeView view,
                   const gfx::Point& origin,
                   ConfirmBubbleModel* model);

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleModel);
};

#endif  // CHROME_BROWSER_UI_CONFIRM_BUBBLE_MODEL_H_
