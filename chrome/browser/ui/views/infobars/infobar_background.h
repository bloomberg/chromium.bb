// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BACKGROUND_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "views/background.h"

class InfoBarBackground : public views::Background {
 public:
  explicit InfoBarBackground(InfoBarDelegate::Type infobar_type);

  // Overridden from views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const;

 private:
  scoped_ptr<views::Background> gradient_background_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarBackground);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BACKGROUND_H_
