// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_VALIDATION_MESSAGE_BUBBLE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_VALIDATION_MESSAGE_BUBBLE_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/validation_message_bubble.h"

@class ValidationMessageBubbleController;

namespace content {
class WebContents;
}

class ValidationMessageBubbleCocoa
    : public ValidationMessageBubble,
      public base::SupportsWeakPtr<ValidationMessageBubbleCocoa> {
 public:
  ValidationMessageBubbleCocoa(content::WebContents* web_contents,
                               const gfx::Rect& anchor_in_root_view,
                               const base::string16& main_text,
                               const base::string16& sub_text);
  ~ValidationMessageBubbleCocoa() override;

  // ValidationMessageBubble overrides:
  void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_root_view) override;
  void CloseValidationMessage() override;

 private:
  base::scoped_nsobject<ValidationMessageBubbleController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageBubbleCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_VALIDATION_MESSAGE_BUBBLE_COCOA_H_
