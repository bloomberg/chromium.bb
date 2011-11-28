// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BUTTON_BORDER_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BUTTON_BORDER_H_
#pragma once

#include "ui/views/controls/button/text_button.h"

// A TextButtonBorder that is dark and also paints the button frame in the
// normal state.
class InfoBarButtonBorder : public views::TextButtonBorder {
 public:
  InfoBarButtonBorder();

 private:
  virtual ~InfoBarButtonBorder();

  DISALLOW_COPY_AND_ASSIGN(InfoBarButtonBorder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_BUTTON_BORDER_H_
