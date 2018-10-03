// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/new_unified_message_center_view.h"

#include "ash/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

NewUnifiedMessageCenterView::NewUnifiedMessageCenterView()
    : scroller_(new views::ScrollView()),
      message_list_view_(new UnifiedMessageListView(this)) {
  message_list_view_->Init();

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetContents(message_list_view_);
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(new MessageCenterScrollBar(this));
  scroller_->set_draw_overflow_indicator(false);
  AddChildView(scroller_);
}

NewUnifiedMessageCenterView::~NewUnifiedMessageCenterView() = default;

void NewUnifiedMessageCenterView::SetMaxHeight(int max_height) {
  scroller_->ClipHeightTo(0, max_height);
}

void NewUnifiedMessageCenterView::ListPreferredSizeChanged() {
  SetVisible(message_list_view_->child_count() > 0);
  PreferredSizeChanged();

  if (GetWidget())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void NewUnifiedMessageCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void NewUnifiedMessageCenterView::OnMessageCenterScrolled() {}

}  // namespace ash
