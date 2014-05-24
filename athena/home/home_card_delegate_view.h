// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_HOME_CARD_DELEGATE_VIEW_H_
#define ATHENA_HOME_HOME_CARD_DELEGATE_VIEW_H_

#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace athena {

class HomeCardDelegateView : public views::WidgetDelegate, public views::View {
 public:
  HomeCardDelegateView();
  virtual ~HomeCardDelegateView();

 private:
  // views::View
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // views::WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(HomeCardDelegateView);
};

}  // namespace athena

#endif  // ATHENA_HOME_HOME_CARD_DELEGATE_VIEW_H_
