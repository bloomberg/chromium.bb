// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/views/controls/link_listener.h"

class ConfirmInfoBarDelegate;
class ElevationIconSetter;

namespace views {
class Button;
class Label;
class MdTextButton;
}

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link.  This is commonly used to do things like:
// "Would you like to do X?  [Yes]  [No]               _Learn More_ [x]"
class ConfirmInfoBar : public InfoBarView,
                       public views::LinkListener {
 public:
  explicit ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate);
  ~ConfirmInfoBar() override;

 private:
  // InfoBarView:
  void Layout() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  int ContentMinimumWidth() const override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  ConfirmInfoBarDelegate* GetDelegate();

  // Returns the width of all content other than the label and link.  Layout()
  // uses this to determine how much space the label and link can take.
  int NonLabelWidth() const;

  views::Label* label_;
  views::MdTextButton* ok_button_;
  views::MdTextButton* cancel_button_;
  views::Link* link_;
  std::unique_ptr<ElevationIconSetter> elevation_icon_setter_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_CONFIRM_INFOBAR_H_
