// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_wrapper.h"

#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/wm/window_properties.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

TrayBubbleWrapper::TrayBubbleWrapper(TrayBackgroundView* tray,
                                     views::TrayBubbleView* bubble_view)
    : tray_(tray),
      bubble_view_(bubble_view) {
  DCHECK(tray_);
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  bubble_widget_->GetNativeView()->
      SetProperty(internal::kStayInSameRootWindowKey, true);

  tray_->InitializeBubbleAnimations(bubble_widget_);
  tray_->UpdateBubbleViewArrow(bubble_view_);
  bubble_view_->InitializeAndShowBubble();

  tray->tray_event_filter()->AddWrapper(this);
}

TrayBubbleWrapper::~TrayBubbleWrapper() {
  tray_->tray_event_filter()->RemoveWrapper(this);
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

void TrayBubbleWrapper::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_ = NULL;

  // Do not call HideBubbleWithView directly but post the task to ensure that
  // HideBubbleWithView is called after the click event on the tray button is
  // handled. See crbug.com/177075 and crbug.com/169940
  MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&TrayBackgroundView::HideBubbleWithView,
                 base::Unretained(tray_), base::Unretained(bubble_view_)));
}

}  // namespace internal
}  // namespace ash
