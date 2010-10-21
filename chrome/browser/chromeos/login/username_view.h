// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERNAME_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERNAME_VIEW_H_
#pragma once

#include <string>

#include "views/view.h"

namespace gfx {
class Font;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace chromeos {
// View that contains two parts. First one is a label with username, second one
// is an empty view with gradient transparency background.
class UsernameView : public views::View {
 public:
  explicit UsernameView(const std::wstring& username);
  virtual ~UsernameView() {}

  // Set the font of the username text.
  void SetFont(const gfx::Font& font);

  // Returns username label.
  views::Label* label() const {
    return label_;
  }

  // views::View overrides.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  views::Label* label_;
  views::View* gradient_;

  DISALLOW_COPY_AND_ASSIGN(UsernameView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERNAME_VIEW_H_
