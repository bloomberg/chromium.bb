// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/validation_message_bubble.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace content {
class WebContents;
}

// A ValidationMessageBubble implementation for Views.
class ValidationMessageBubbleView
    : public views::BubbleDialogDelegateView,
      public ValidationMessageBubble,
      public base::SupportsWeakPtr<ValidationMessageBubbleView> {
 public:
  ValidationMessageBubbleView(content::WebContents* web_contents,
                              const gfx::Rect& anchor_in_root_view,
                              const base::string16& main_text,
                              const base::string16& sub_text);
  ~ValidationMessageBubbleView() override;

  // BubbleDialogDelegateView overrides:
  gfx::Size GetPreferredSize() const override;
  int GetDialogButtons() const override;

  // ValidationMessageBubble overrides:
  void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view) override;
  void CloseValidationMessage() override;

 private:
  gfx::Rect RootViewToScreenRect(
      const gfx::Rect& rect_in_root_view,
      const content::RenderWidgetHostView* render_widget_host_view) const;

  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_VIEW_H_
