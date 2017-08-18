// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/message_center_bubble.h"

#include "base/macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::MessageCenterTray;
using message_center::MessageCenterView;

namespace ash {

namespace {
const int kDefaultMaxHeight = 400;
}

// ContentsView ////////////////////////////////////////////////////////////////

// Handles changes in MessageCenterView sizes.
class ContentsView : public views::View {
 public:
  explicit ContentsView(MessageCenterBubble* bubble, views::View* contents);
  ~ContentsView() override;

  // Overridden from views::View:
  int GetHeightForWidth(int width) const override;

 protected:
  // Overridden from views::View:
  void ChildPreferredSizeChanged(View* child) override;

 private:
  base::WeakPtr<MessageCenterBubble> bubble_;

  DISALLOW_COPY_AND_ASSIGN(ContentsView);
};

ContentsView::ContentsView(MessageCenterBubble* bubble, views::View* contents)
    : bubble_(bubble->AsWeakPtr()) {
  SetLayoutManager(new views::FillLayout());
  AddChildView(contents);
}

ContentsView::~ContentsView() {}

int ContentsView::GetHeightForWidth(int width) const {
  DCHECK_EQ(1, child_count());
  int contents_width = std::max(width - GetInsets().width(), 0);
  int contents_height = child_at(0)->GetHeightForWidth(contents_width);
  return contents_height + GetInsets().height();
}

void ContentsView::ChildPreferredSizeChanged(View* child) {
  // TODO(dharcourt): Reduce the amount of updating this requires.
  if (bubble_.get())
    bubble_->bubble_view()->UpdateBubble();
}

// MessageCenterBubble /////////////////////////////////////////////////////////

MessageCenterBubble::MessageCenterBubble(MessageCenter* message_center,
                                         MessageCenterTray* tray)
    : message_center_(message_center),
      tray_(tray),
      max_height_(kDefaultMaxHeight) {}

MessageCenterBubble::~MessageCenterBubble() {
  // Removs this from the widget observers just in case. MessageCenterBubble
  // might be destoryed without calling its Widget's Close/CloseNow.
  if (bubble_view_ && bubble_view_->GetWidget())
    bubble_view_->GetWidget()->RemoveObserver(this);
  if (bubble_view_)
    bubble_view_->ResetDelegate();
}

void MessageCenterBubble::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  message_center_view_ = nullptr;
}

void MessageCenterBubble::SetMaxHeight(int height) {
  // Maximum height makes sense only for the new design.
  if (height == 0)
    height = kDefaultMaxHeight;
  if (height == max_height_)
    return;

  max_height_ = height;
  if (bubble_view_)
    bubble_view_->SetMaxHeight(max_height_);
}

void MessageCenterBubble::SetSettingsVisible() {
  if (message_center_view_)
    message_center_view_->SetSettingsVisible(true);
  else
    initially_settings_visible_ = true;
}

void MessageCenterBubble::InitializeContents(
    views::TrayBubbleView* new_bubble_view) {
  bubble_view_ = new_bubble_view;
  bubble_view_->GetWidget()->AddObserver(this);
  message_center_view_ = new MessageCenterView(
      message_center_, tray_, max_height_, initially_settings_visible_);
  bubble_view_->AddChildView(new ContentsView(this, message_center_view_));
  message_center_view_->Init();
  // Resize the content of the bubble view to the given bubble size. This is
  // necessary in case of the bubble border forcing a bigger size then the
  // |new_bubble_view| actually wants. See crbug.com/169390.
  bubble_view_->Layout();
  UpdateBubbleView();
}

void MessageCenterBubble::OnWidgetClosing(views::Widget* widget) {
  if (bubble_view_ && bubble_view_->GetWidget())
    bubble_view_->GetWidget()->RemoveObserver(this);
  if (message_center_view_)
    message_center_view_->SetIsClosing(true);
}

bool MessageCenterBubble::IsVisible() const {
  return bubble_view() && bubble_view()->GetWidget()->IsVisible();
}

size_t MessageCenterBubble::NumMessageViewsForTest() const {
  return message_center_view_->NumMessageViewsForTest();
}

void MessageCenterBubble::UpdateBubbleView() {
  if (!bubble_view_)
    return;  // Could get called after view is closed
  message_center_view_->SetNotifications(
      message_center_->GetVisibleNotifications());
  bubble_view_->GetWidget()->Show();
  bubble_view_->UpdateBubble();
}

}  // namespace ash
