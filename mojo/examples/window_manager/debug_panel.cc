// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/window_manager/debug_panel.h"

#include "base/strings/utf_string_conversions.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/views/native_widget_view_manager.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/widget/widget.h"

namespace mojo {
namespace examples {

namespace {

const int kControlBorderInset = 5;
const int kNavigationTargetGroupId = 1;

}  // namespace

DebugPanel::DebugPanel(view_manager::Node* node)
    : navigation_target_label_(new views::Label(
          base::ASCIIToUTF16("Navigation target:"))),
      navigation_target_new_(new views::RadioButton(
          base::ASCIIToUTF16("New window"), kNavigationTargetGroupId)),
      navigation_target_source_(new views::RadioButton(
          base::ASCIIToUTF16("Source window"), kNavigationTargetGroupId)),
      navigation_target_default_(new views::RadioButton(
          base::ASCIIToUTF16("Default"), kNavigationTargetGroupId)) {
  navigation_target_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  navigation_target_default_->SetChecked(true);

  views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView();
  widget_delegate->GetContentsView()->set_background(
      views::Background::CreateSolidBackground(0xFFDDDDDD));
  widget_delegate->GetContentsView()->AddChildView(navigation_target_label_);
  widget_delegate->GetContentsView()->AddChildView(navigation_target_default_);
  widget_delegate->GetContentsView()->AddChildView(navigation_target_new_);
  widget_delegate->GetContentsView()->AddChildView(navigation_target_source_);
  widget_delegate->GetContentsView()->SetLayoutManager(this);

  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = new NativeWidgetViewManager(widget, node);
  params.delegate = widget_delegate;
  params.bounds = gfx::Rect(node->bounds().size());
  widget->Init(params);
  widget->Show();
}

DebugPanel::~DebugPanel() {
}

gfx::Size DebugPanel::GetPreferredSize(const views::View* view) const {
  return gfx::Size();
}

navigation::Target DebugPanel::navigation_target() const {
  if (navigation_target_new_->checked())
    return navigation::NEW_NODE;
  if (navigation_target_source_->checked())
    return navigation::SOURCE_NODE;
  return navigation::DEFAULT;
}

void DebugPanel::Layout(views::View* view) {
  int y = kControlBorderInset;
  int w = view->width() - kControlBorderInset * 2;

  navigation_target_label_->SetBounds(
      kControlBorderInset, y, w,
      navigation_target_label_->GetPreferredSize().height());
  y += navigation_target_label_->height();

  views::RadioButton* radios[] = {
    navigation_target_default_,
    navigation_target_new_,
    navigation_target_source_,
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(radios); ++i) {
    radios[i]->SetBounds(kControlBorderInset, y, w,
                         radios[i]->GetPreferredSize().height());
    y += radios[i]->height();
  }
}

}  // namespace examples
}  // namespace mojo
