// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error.h"

#include "base/logging.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

// GlobalError ---------------------------------------------------------------

GlobalError::GlobalError() {}

GlobalError::~GlobalError() {}

GlobalError::Severity GlobalError::GetSeverity() { return SEVERITY_MEDIUM; }

int GlobalError::MenuItemIconResourceID() {
  // If you change this make sure to also change the bubble icon and the wrench
  // icon color.
  return IDR_INPUT_ALERT_MENU;
}

// GlobalErrorWithStandardBubble ---------------------------------------------

GlobalErrorWithStandardBubble::GlobalErrorWithStandardBubble()
    : has_shown_bubble_view_(false), bubble_view_(NULL) {}

GlobalErrorWithStandardBubble::~GlobalErrorWithStandardBubble() {}

bool GlobalErrorWithStandardBubble::HasBubbleView() { return true; }

bool GlobalErrorWithStandardBubble::HasShownBubbleView() {
  return has_shown_bubble_view_;
}

void GlobalErrorWithStandardBubble::ShowBubbleView(Browser* browser) {
  has_shown_bubble_view_ = true;
#if defined(OS_ANDROID)
  // http://crbug.com/136506
  NOTIMPLEMENTED() << "Chrome for Android doesn't support global errors";
#else
  bubble_view_ =
      GlobalErrorBubbleViewBase::ShowStandardBubbleView(browser, AsWeakPtr());
#endif
}

GlobalErrorBubbleViewBase* GlobalErrorWithStandardBubble::GetBubbleView() {
  return bubble_view_;
}

bool GlobalErrorWithStandardBubble::ShouldCloseOnDeactivate() const {
  return true;
}

gfx::Image GlobalErrorWithStandardBubble::GetBubbleViewIcon() {
  // If you change this make sure to also change the menu icon and the wrench
  // icon color.
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INPUT_ALERT);
}

void GlobalErrorWithStandardBubble::BubbleViewDidClose(Browser* browser) {
  DCHECK(browser);
  bubble_view_ = NULL;
  OnBubbleViewDidClose(browser);
}
