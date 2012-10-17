// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error.h"

#include "base/logging.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "grit/theme_resources.h"

GlobalError::GlobalError()
    : has_shown_bubble_view_(false),
      bubble_view_(NULL) {
}

GlobalError::~GlobalError() {
}

int GlobalError::GetBadgeResourceID() {
  return IDR_UPDATE_BADGE4;
}

GlobalError::Severity GlobalError::GetSeverity() {
  switch (GetBadgeResourceID()) {
    case IDR_UPDATE_BADGE: return SEVERITY_LOW;
    case IDR_UPDATE_BADGE2: return SEVERITY_MEDIUM;
    case IDR_UPDATE_BADGE3: return SEVERITY_HIGH;
    case IDR_UPDATE_BADGE4: return SEVERITY_CRITICAL;
    default: break;
  }
  return SEVERITY_LOW;
}

int GlobalError::MenuItemIconResourceID() {
  return IDR_UPDATE_MENU4;
}

bool GlobalError::HasShownBubbleView() {
  return has_shown_bubble_view_;
}

void GlobalError::ShowBubbleView(Browser* browser) {
  has_shown_bubble_view_ = true;
#if defined(OS_ANDROID)
  // http://crbug.com/136506
  NOTIMPLEMENTED() << "Chrome for Android doesn't support global errors";
#else
  bubble_view_ =
      GlobalErrorBubbleViewBase::ShowBubbleView(browser, AsWeakPtr());
#endif
}

GlobalErrorBubbleViewBase* GlobalError::GetBubbleView() {
  return bubble_view_;
}

int GlobalError::GetBubbleViewIconResourceID() {
  return IDR_INPUT_ALERT;
}

void GlobalError::BubbleViewDidClose(Browser* browser) {
  DCHECK(browser);
  bubble_view_ = NULL;
  OnBubbleViewDidClose(browser);
}
