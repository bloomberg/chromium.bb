// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERNAME_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERNAME_VIEW_H_
#pragma once

#include <string>

#include "views/controls/label.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Forward declaration.
namespace gfx {
  class Rect;
}

namespace chromeos {
// Label with customized paddings and long text fade out.
class UsernameView : public views::Label {
 public:
  UsernameView(const std::wstring& username);
  virtual ~UsernameView() {}

  // Overriden from views::Label.
  virtual void Paint(gfx::Canvas* canvas);

 private:
  // Paints username to the bitmap with the bounds given.
  void PaintUsername(const gfx::Rect& bounds);

  // Holds painted username.
  scoped_ptr<SkBitmap> text_image_;

  // Holds width of the text drawn.
  int text_width_;

  // Holds margins width (depends on the height).
  int margin_width_;

  DISALLOW_COPY_AND_ASSIGN(UsernameView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERNAME_VIEW_H_
