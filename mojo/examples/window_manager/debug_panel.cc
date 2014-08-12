// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/window_manager/debug_panel.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/views/native_widget_view_manager.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/widget/widget.h"

namespace mojo {
namespace examples {

namespace {

const int kControlBorderInset = 5;
const int kNavigationTargetGroupId = 1;

}  // namespace

DebugPanel::DebugPanel(Delegate* delegate, View* view)
    : delegate_(delegate),
      view_(view),
      navigation_target_label_(new views::Label(
          base::ASCIIToUTF16("Navigation target:"))),
      navigation_target_new_(new views::RadioButton(
          base::ASCIIToUTF16("New window"), kNavigationTargetGroupId)),
      navigation_target_source_(new views::RadioButton(
          base::ASCIIToUTF16("Source window"), kNavigationTargetGroupId)),
      navigation_target_default_(new views::RadioButton(
          base::ASCIIToUTF16("Default"), kNavigationTargetGroupId)),
      next_color_(0),
      colored_square_(new views::BlueButton(
          this, base::ASCIIToUTF16("Local nav test"))),
      close_last_(new views::BlueButton(
          this, base::ASCIIToUTF16("Close last window"))),
      cross_app_(new views::BlueButton(
          this, base::ASCIIToUTF16("Cross-app nav test"))) {
  navigation_target_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  navigation_target_default_->SetChecked(true);

  views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView();
  widget_delegate->GetContentsView()->set_background(
      views::Background::CreateSolidBackground(0xFFDDDDDD));
  widget_delegate->GetContentsView()->AddChildView(navigation_target_label_);
  widget_delegate->GetContentsView()->AddChildView(navigation_target_default_);
  widget_delegate->GetContentsView()->AddChildView(navigation_target_new_);
  widget_delegate->GetContentsView()->AddChildView(navigation_target_source_);
  widget_delegate->GetContentsView()->AddChildView(colored_square_);
  widget_delegate->GetContentsView()->AddChildView(close_last_);
  widget_delegate->GetContentsView()->AddChildView(cross_app_);
  widget_delegate->GetContentsView()->SetLayoutManager(this);

  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = new NativeWidgetViewManager(widget, view);
  params.delegate = widget_delegate;
  params.bounds = gfx::Rect(view->bounds().size());
  widget->Init(params);
  widget->Show();
}

DebugPanel::~DebugPanel() {
}

gfx::Size DebugPanel::GetPreferredSize(const views::View* view) const {
  return gfx::Size();
}

Target DebugPanel::navigation_target() const {
  if (navigation_target_new_->checked())
    return TARGET_NEW_NODE;
  if (navigation_target_source_->checked())
    return TARGET_SOURCE_NODE;
  return TARGET_DEFAULT;
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

  y += kControlBorderInset;
  views::Button* buttons[] = {
    colored_square_,
    close_last_,
    cross_app_,
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(buttons); ++i) {
    buttons[i]->SetBounds(kControlBorderInset, y, w,
                          buttons[i]->GetPreferredSize().height());
    y += buttons[i]->height();
  }
}

void DebugPanel::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == colored_square_) {
    Navigate(base::StringPrintf("mojo://mojo_embedded_app/%x",
                                kColors[next_color_ % arraysize(kColors)]));
    next_color_++;
  } else if (sender == close_last_) {
    delegate_->CloseTopWindow();
  } else if (sender == cross_app_) {
    Navigate("http://www.aaronboodman.com/z_dropbox/test.html");
  }
}

void DebugPanel::Navigate(const std::string& url) {
  NavigationDetailsPtr details(NavigationDetails::New());
  details->request->url = url;
  delegate_->RequestNavigate(view_->id(), TARGET_NEW_NODE, details.Pass());
}

}  // namespace examples
}  // namespace mojo
