// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_COPY_BACKGROUND_H_
#define CHROME_BROWSER_CHROMEOS_VIEWS_COPY_BACKGROUND_H_
#pragma once

#include "views/background.h"

namespace views {
class View;
}  // namespace views

namespace chromeos {

// A background that copies the background renderer from
// other (typically one of parent chain) view.
class CopyBackground : public views::Background {
 public:
  explicit CopyBackground(views::View* copy_from);
  virtual ~CopyBackground() {}

  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const;

 private:
  views::View* background_owner_;

  DISALLOW_COPY_AND_ASSIGN(CopyBackground);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VIEWS_COPY_BACKGROUND_H_
