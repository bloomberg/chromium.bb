// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/validation_message_bubble_view.h"

#include "chrome/grit/theme_resources.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace {

const int kWindowMinWidth = 64;
const int kWindowMaxWidth = 256;
const int kIconTextMargin = 8;
const int kTextVerticalMargin = 4;

}  // namespace

ValidationMessageBubbleView::ValidationMessageBubbleView(
    content::WebContents* web_contents,
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  content::RenderWidgetHostView* render_widget_host_view =
      web_contents->GetRenderWidgetHostView();
  set_parent_window(render_widget_host_view->GetNativeView());

  set_can_activate(false);
  set_arrow(views::BubbleBorder::TOP_LEFT);
  SetAnchorRect(
      RootViewToScreenRect(anchor_in_root_view, render_widget_host_view));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(*bundle.GetImageSkiaNamed(IDR_INPUT_ALERT));
  icon->SizeToPreferredSize();
  AddChildView(icon);

  views::Label* label = new views::Label(
      main_text, bundle.GetFontList(ui::ResourceBundle::MediumFont));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  int text_start_x = icon->bounds().right() + kIconTextMargin;
  int min_available = kWindowMinWidth - text_start_x;
  int max_available = kWindowMaxWidth - text_start_x;
  int label_width = label->GetPreferredSize().width();
  label->SetMultiLine(true);
  AddChildView(label);

  views::Label* sub_label = nullptr;
  if (!sub_text.empty()) {
    sub_label = new views::Label(sub_text);
    sub_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_width = std::max(label_width, sub_label->GetPreferredSize().width());
    sub_label->SetMultiLine(true);
    AddChildView(sub_label);
  }

  label_width = std::min(std::max(label_width, min_available), max_available);
  label->SetBounds(text_start_x, 0,
                   label_width, label->GetHeightForWidth(label_width));
  int content_bottom = label->height();

  if (sub_label) {
    sub_label->SetBounds(text_start_x,
                         content_bottom + kTextVerticalMargin,
                         label_width,
                         sub_label->GetHeightForWidth(label_width));
    content_bottom += kTextVerticalMargin + sub_label->height();
  }

  size_ = gfx::Size(text_start_x + label_width, content_bottom);

  views::BubbleDialogDelegateView::CreateBubble(this)->ShowInactive();
}

ValidationMessageBubbleView::~ValidationMessageBubbleView() {
}

gfx::Rect ValidationMessageBubbleView::RootViewToScreenRect(
    const gfx::Rect& rect_in_root_view,
    const content::RenderWidgetHostView* render_widget_host_view) const {
  gfx::NativeView view = render_widget_host_view->GetNativeView();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(view);
  const float scale = display.device_scale_factor();
  return gfx::ScaleToEnclosingRect(rect_in_root_view, 1 / scale) +
         render_widget_host_view->GetViewBounds().origin().OffsetFromOrigin();
}

gfx::Size ValidationMessageBubbleView::GetPreferredSize() const {
  return size_;
}

int ValidationMessageBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void ValidationMessageBubbleView::SetPositionRelativeToAnchor(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_root_view) {
  SetAnchorRect(
      RootViewToScreenRect(anchor_in_root_view, widget_host->GetView()));
}

void ValidationMessageBubbleView::CloseValidationMessage() {
  GetWidget()->Close();
}
