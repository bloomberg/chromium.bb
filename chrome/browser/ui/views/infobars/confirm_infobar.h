// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/views/controls/link_listener.h"

class ConfirmInfoBarDelegate;

namespace views {
class Label;
class TextButton;
}

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link.  This is commonly used to do things like:
// "Would you like to do X?  [Yes]  [No]               _Learn More_ [x]"
class ConfirmInfoBar : public InfoBarView,
                       public views::LinkListener {
 public:
  ConfirmInfoBar(InfoBarTabHelper* owner, ConfirmInfoBarDelegate* delegate);

 protected:
  // TODO(rogerta): These only need to be protected due to the
  // OneClickLoginInfoBar experiment and can be made private once that's
  // removed.
  virtual ~ConfirmInfoBar();

  views::TextButton* ok_button() { return ok_button_; }

  // InfoBarView:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  ConfirmInfoBarDelegate* GetDelegate();

  views::Label* label_;
  views::TextButton* ok_button_;
  views::TextButton* cancel_button_;
  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
