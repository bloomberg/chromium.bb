// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_VIEW_H_

namespace chromeos {

class ResetScreen;

// Interface for dependency injection between ResetScreen and its actual
// representation, either views based or WebUI.
class ResetView {
 public:
  virtual ~ResetView() {}

  virtual void Bind(ResetScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_VIEW_H_
