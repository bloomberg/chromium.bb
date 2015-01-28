// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/validation_message_bubble_view.h"

#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

ValidationMessageBubbleView::ValidationMessageBubbleView(
    content::WebContents* web_contents,
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  const gfx::Rect anchor_in_screen =
      anchor_in_root_view + rwhv->GetViewBounds().origin().OffsetFromOrigin();
  delegate_ = new ValidationMessageBubbleDelegate(
      anchor_in_screen, main_text, sub_text, this);
  delegate_->set_parent_window(rwhv->GetNativeView());
  views::BubbleDelegateView::CreateBubble(delegate_);
  delegate_->GetWidget()->ShowInactive();
}

ValidationMessageBubbleView::~ValidationMessageBubbleView() {
  if (delegate_)
    delegate_->Close();
}

void ValidationMessageBubbleView::SetPositionRelativeToAnchor(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_root_view) {
  if (!delegate_)
    return;
  delegate_->SetPositionRelativeToAnchor(
      anchor_in_root_view +
      widget_host->GetView()->GetViewBounds().origin().OffsetFromOrigin());
}

void ValidationMessageBubbleView::WindowClosing() {
  delegate_ = NULL;
}
