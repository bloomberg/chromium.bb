// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_LABEL_BUTTON_BORDER_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_LABEL_BUTTON_BORDER_H_

#include "ui/views/controls/button/label_button_border.h"

// A LabelButtonBorder that is dark and also paints the button frame in the
// normal state.
class InfoBarLabelButtonBorder : public views::LabelButtonBorder {
 public:
  InfoBarLabelButtonBorder();

 private:
  virtual ~InfoBarLabelButtonBorder();

  // Overridden from views::LabelButtonBorder:
  virtual gfx::Insets GetInsets() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InfoBarLabelButtonBorder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_LABEL_BUTTON_BORDER_H_
