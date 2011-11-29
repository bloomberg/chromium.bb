// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_LINK_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_LINK_INFOBAR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/views/controls/link_listener.h"

class LinkInfoBarDelegate;

// An infobar that shows a string with an embedded link.
class LinkInfoBar : public InfoBarView,
                    public views::LinkListener {
 public:
  LinkInfoBar(InfoBarTabHelper* owner, LinkInfoBarDelegate* delegate);

 private:
  virtual ~LinkInfoBar();

  // InfoBarView:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  LinkInfoBarDelegate* GetDelegate();

  views::Label* label_1_;
  views::Link* link_;
  views::Label* label_2_;

  DISALLOW_COPY_AND_ASSIGN(LinkInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_LINK_INFOBAR_H_
