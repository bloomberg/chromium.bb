// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_TEXT_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_TEXT_BUTTON_H_
#pragma once

#include "views/controls/button/text_button.h"

//  A TextButton subclass that overrides OnMousePressed to default to
//  CustomButton so as to create pressed state effect.

class InfoBarTextButton : public views::TextButton {
 public:
  // Creates a button with the specified |text|.
  static InfoBarTextButton* Create(views::ButtonListener* listener,
                                   const string16& text);
  // Creates a button which text is the resource string identified by
  // |message_id|.
  static InfoBarTextButton* CreateWithMessageID(views::ButtonListener* listener,
                                                int message_id);
  static InfoBarTextButton* CreateWithMessageIDAndParam(
      views::ButtonListener* listener, int message_id, const string16& param);

  virtual ~InfoBarTextButton();

 protected:
  InfoBarTextButton(views::ButtonListener* listener, const string16& text);

  // Overriden from TextButton:
  virtual bool OnMousePressed(const views::MouseEvent& e);

 private:
  DISALLOW_COPY_AND_ASSIGN(InfoBarTextButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_TEXT_BUTTON_H_
