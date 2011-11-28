// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SHUTDOWN_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SHUTDOWN_BUTTON_H_
#pragma once

#include "ui/views/controls/button/text_button.h"

namespace chromeos {

class ShutdownButton : public views::TextButton,
                       public views::ButtonListener {
 public:
  ShutdownButton();

  // Initializes shutdown button.
  void Init();

  // Layout the shutdown button at the right bottom corner of
  // |parent|.
  void LayoutIn(views::View* parent);

 private:
  // views::View overrides.
  virtual gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ShutdownButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SHUTDOWN_BUTTON_H_
