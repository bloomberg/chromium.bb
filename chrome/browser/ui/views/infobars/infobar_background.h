// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BACKGROUND_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/views/background.h"

class InfoBarBackground : public views::Background {
 public:
  explicit InfoBarBackground(infobars::InfoBarDelegate::Type infobar_type);
  ~InfoBarBackground() override;

 private:
  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  DISALLOW_COPY_AND_ASSIGN(InfoBarBackground);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BACKGROUND_H_
