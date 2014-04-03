// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_wrapper.h"

#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

TrayBubbleWrapper::TrayBubbleWrapper(TrayBackgroundView* tray,
                                     views::TrayBubbleView* bubble_view)
    : tray_(tray),
      bubble_view_(bubble_view) {
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

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
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = NULL;

  // Although the bubble is already closed, the next mouse release event
  // will invoke PerformAction which reopens the bubble again. To prevent the
  // reopen, the mouse capture of |tray_| has to be released.
  // See crbug.com/177075
  aura::client::CaptureClient* capture_client = aura::client::GetCaptureClient(
      tray_->GetWidget()->GetNativeView()->GetRootWindow());
  if (capture_client)
    capture_client->ReleaseCapture(tray_->GetWidget()->GetNativeView());
  tray_->HideBubbleWithView(bubble_view_);  // May destroy |bubble_view_|
}

void TrayBubbleWrapper::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  DCHECK_EQ(bubble_widget_, widget);
  tray_->BubbleResized(bubble_view_);
}

}  // namespace ash
