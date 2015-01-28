// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_VIEW_H_

#include "chrome/browser/ui/validation_message_bubble.h"
#include "chrome/browser/ui/views/validation_message_bubble_delegate.h"

namespace content {
class WebContents;
}

// A ValidationMessageBubble implementation for Views.
class ValidationMessageBubbleView
    : public ValidationMessageBubble,
      public ValidationMessageBubbleDelegate::Observer {
 public:
  ValidationMessageBubbleView(content::WebContents* web_contents,
                              const gfx::Rect& anchor_in_root_view,
                              const base::string16& main_text,
                              const base::string16& sub_text);
  ~ValidationMessageBubbleView() override;

  // ValidationMessageBubble overrides:
  void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view) override;

  // ValidationMessageBubbleDelegate::Observer overrides:
  void WindowClosing() override;

 private:
  ValidationMessageBubbleDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_VIEW_H_
