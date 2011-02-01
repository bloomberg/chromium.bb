// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#pragma once

#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "views/controls/link.h"

class ConfirmInfoBarDelegate;
class InfoBarTextButton;

namespace views {
class Label;
}

// TODO(pkasting): This class will die soon.
class AlertInfoBar : public InfoBarView {
 public:
  explicit AlertInfoBar(ConfirmInfoBarDelegate* delegate);
  virtual ~AlertInfoBar();

  // Overridden from views::View:
  virtual void Layout();

 protected:
  views::Label* label() const { return label_; }
  views::ImageView* icon() const { return icon_; }

 private:
  views::Label* label_;
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(AlertInfoBar);
};

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link.  This is commonly used to do things like:
// "Would you like to do X?  [Yes]  [No]               _Learn More_ [x]"
// TODO(pkasting): The above layout is the desired, but not current, layout; fix
// coming in a future patch.
class ConfirmInfoBar : public AlertInfoBar,
                       public views::LinkController  {
 public:
  explicit ConfirmInfoBar(ConfirmInfoBarDelegate* delegate);
  virtual ~ConfirmInfoBar();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::View:
  virtual void Layout();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

 private:
  void Init();

  ConfirmInfoBarDelegate* GetDelegate();

  InfoBarTextButton* ok_button_;
  InfoBarTextButton* cancel_button_;
  views::Link* link_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
