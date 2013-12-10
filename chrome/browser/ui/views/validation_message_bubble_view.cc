// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/validation_message_bubble.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/views/validation_message_bubble_delegate.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/views/widget/widget.h"

namespace {

// A ValidationMessageBubble implementation for Views.
class ValidationMessageBubbleImpl
    : public chrome::ValidationMessageBubble,
      public ValidationMessageBubbleDelegate::Observer {
 public:
  ValidationMessageBubbleImpl(content::RenderWidgetHost* widget_host,
                              const gfx::Rect& anchor_in_screen,
                              const base::string16& main_text,
                              const base::string16& sub_text);

  virtual ~ValidationMessageBubbleImpl() {
    if (delegate_ != NULL)
      delegate_->Close();
  }

  virtual void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view) OVERRIDE {
    if (!delegate_)
      return;
    delegate_->SetPositionRelativeToAnchor(anchor_in_root_view +
        widget_host->GetView()->GetViewBounds().origin().OffsetFromOrigin());
  }

  // ValidationMessageBubbleDelegate::Observer override:
  virtual void WindowClosing() OVERRIDE {
    delegate_ = NULL;
  }

 private:
  ValidationMessageBubbleDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageBubbleImpl);
};

ValidationMessageBubbleImpl::ValidationMessageBubbleImpl(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_screen,
    const base::string16& main_text,
    const base::string16& sub_text) {
  delegate_ = new ValidationMessageBubbleDelegate(
      anchor_in_screen, main_text, sub_text, this);
  delegate_->set_parent_window(platform_util::GetTopLevel(
      widget_host->GetView()->GetNativeView()));
  views::BubbleDelegateView::CreateBubble(delegate_);
  delegate_->GetWidget()->ShowInactive();
}

}  // namespace

namespace chrome {

scoped_ptr<ValidationMessageBubble> ValidationMessageBubble::CreateAndShow(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  const gfx::Rect anchor_in_screen = anchor_in_root_view
      + widget_host->GetView()->GetViewBounds().origin().OffsetFromOrigin();
  scoped_ptr<ValidationMessageBubble> bubble(new ValidationMessageBubbleImpl(
      widget_host, anchor_in_screen, main_text, sub_text));
  return bubble.Pass();
}

}  // namespace chrome
