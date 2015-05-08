// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/browser/desktop/desktop_ui.h"

#include "base/strings/string16.h"
#include "mandoline/ui/aura/native_widget_view_manager.h"
#include "mandoline/ui/browser/browser.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget_delegate.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// DesktopUI, public:

DesktopUI::DesktopUI(Browser* browser, mojo::Shell* shell)
    : browser_(browser),
      shell_(shell),
      omnibox_(new views::Textfield),
      root_(nullptr),
      content_(nullptr) {
  omnibox_->set_controller(this);
}

DesktopUI::~DesktopUI() {}

////////////////////////////////////////////////////////////////////////////////
// DesktopUI, BrowserUI implementation:

void DesktopUI::Init(mojo::View* root, mojo::View* content) {
  root_ = root;
  content_ = content;

  views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
  widget_delegate->GetContentsView()->set_background(
    views::Background::CreateSolidBackground(0xFFDDDDDD));
  widget_delegate->GetContentsView()->AddChildView(omnibox_);
  widget_delegate->GetContentsView()->SetLayoutManager(this);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = new NativeWidgetViewManager(widget, shell_, root_);
  params.delegate = widget_delegate;
  params.bounds = root_->bounds().To<gfx::Rect>();
  widget->Init(params);
  widget->Show();
  root_->SetFocus();
  omnibox_->RequestFocus();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopUI, views::LayoutManager implementation:

gfx::Size DesktopUI::GetPreferredSize(const views::View* view) const {
  return gfx::Size();
}

void DesktopUI::Layout(views::View* host) {
  gfx::Rect omnibox_bounds = host->bounds();
  omnibox_bounds.Inset(10, 10, 10, host->bounds().height() - 40);
  omnibox_->SetBoundsRect(omnibox_bounds);

  mojo::Rect content_bounds_mojo;
  content_bounds_mojo.x = omnibox_bounds.x();
  content_bounds_mojo.y = omnibox_bounds.bottom() + 10;
  content_bounds_mojo.width = omnibox_bounds.width();
  content_bounds_mojo.height =
      host->bounds().height() - content_bounds_mojo.y - 10;
  content_->SetBounds(content_bounds_mojo);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopUI, views::TextfieldController implementation:

bool DesktopUI::HandleKeyEvent(views::Textfield* sender,
                               const ui::KeyEvent& key_event) {
  if (key_event.key_code() == ui::VKEY_RETURN) {
    browser_->ReplaceContentWithURL(
        mojo::String::From<base::string16>(sender->text()));
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserUI, public:

// static
BrowserUI* BrowserUI::Create(Browser* browser, mojo::Shell* shell) {
  return new DesktopUI(browser, shell);
}

}  // namespace mandoline
