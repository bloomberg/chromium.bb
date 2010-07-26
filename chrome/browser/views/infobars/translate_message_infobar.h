// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_H_
#pragma once

#include "chrome/browser/views/infobars/translate_infobar_base.h"

class InfoBarTextButton;

class TranslateMessageInfoBar : public TranslateInfoBarBase {
 public:
  explicit TranslateMessageInfoBar(TranslateInfoBarDelegate* delegate);

  virtual void Layout();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  views::Label* label_;
  InfoBarTextButton* button_;

  DISALLOW_COPY_AND_ASSIGN(TranslateMessageInfoBar);
};

#endif  // CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_H_
