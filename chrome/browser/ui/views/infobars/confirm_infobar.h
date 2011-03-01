// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#pragma once

#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "views/controls/link.h"

class ConfirmInfoBarDelegate;
namespace views {
class Label;
class TextButton;
}

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link.  This is commonly used to do things like:
// "Would you like to do X?  [Yes]  [No]               _Learn More_ [x]"
class ConfirmInfoBar : public InfoBarView,
                       public views::LinkController {
 public:
  explicit ConfirmInfoBar(ConfirmInfoBarDelegate* delegate);

 private:
  virtual ~ConfirmInfoBar();

  // InfoBarView:
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);
  virtual int ContentMinimumWidth() const;

  // views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  ConfirmInfoBarDelegate* GetDelegate();

  views::Label* label_;
  views::TextButton* ok_button_;
  views::TextButton* cancel_button_;
  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
