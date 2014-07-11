// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_widget_delegate.h"

#include "athena/activity/activity_frame_view.h"
#include "athena/activity/public/activity_view_model.h"
#include "ui/views/view.h"
#include "ui/views/window/client_view.h"

namespace athena {

ActivityWidgetDelegate::ActivityWidgetDelegate(ActivityViewModel* view_model)
    : view_model_(view_model) {
}

ActivityWidgetDelegate::~ActivityWidgetDelegate() {
}

base::string16 ActivityWidgetDelegate::GetWindowTitle() const {
  return view_model_->GetTitle();
}

void ActivityWidgetDelegate::DeleteDelegate() {
  delete this;
}

views::Widget* ActivityWidgetDelegate::GetWidget() {
  return GetContentsView()->GetWidget();
}

const views::Widget* ActivityWidgetDelegate::GetWidget() const {
  return const_cast<ActivityWidgetDelegate*>(this)->GetWidget();
}

views::View* ActivityWidgetDelegate::GetContentsView() {
  return view_model_->GetContentsView();
}

views::ClientView* ActivityWidgetDelegate::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, view_model_->GetContentsView());
}

views::NonClientFrameView* ActivityWidgetDelegate::CreateNonClientFrameView(
    views::Widget* widget) {
  return new ActivityFrameView(widget, view_model_);
}

}  // namespace athena
