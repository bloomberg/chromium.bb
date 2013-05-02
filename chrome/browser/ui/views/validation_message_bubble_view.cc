// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/validation_message_bubble.h"

#include "chrome/browser/ui/views/validation_message_bubble_delegate.h"
#include "ui/views/widget/widget.h"

namespace {

// A ValidationMessageBubble implementation for Views.
class ValidationMessageBubbleImpl
    : public chrome::ValidationMessageBubble,
      public ValidationMessageBubbleDelegate::Observer {
 public:
  ValidationMessageBubbleImpl(const gfx::Rect& anchor_in_screen,
                              const string16& main_text,
                              const string16& sub_text);

  virtual ~ValidationMessageBubbleImpl() {
    if (delegate_ != NULL)
      delegate_->Hide();
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
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text) {
  delegate_ = new ValidationMessageBubbleDelegate(
      anchor_in_screen, main_text, sub_text, this);
  views::BubbleDelegateView::CreateBubble(delegate_);
  delegate_->GetWidget()->Show();
}

}  // namespace

namespace chrome {

scoped_ptr<ValidationMessageBubble> ValidationMessageBubble::CreateAndShow(
    content::RenderWidgetHost*,
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text) {
  scoped_ptr<ValidationMessageBubble> bubble(
      new ValidationMessageBubbleImpl(anchor_in_screen, main_text, sub_text));
  return bubble.Pass();
}

}  // namespace chrome
