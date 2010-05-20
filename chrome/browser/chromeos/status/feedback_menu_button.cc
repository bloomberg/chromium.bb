// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/feedback_menu_button.h"

#include <string>

#include "app/resource_bundle.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton

FeedbackMenuButton::FeedbackMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      host_(host) {
  DCHECK(host_);
}

FeedbackMenuButton::~FeedbackMenuButton() {
}

////////////////////////////////////////////////////////////////////////////////
// FeedbackMenuButton, StatusAreaButton implementation:

void FeedbackMenuButton::DrawPressed(gfx::Canvas* canvas) {
  DrawFeedbackIcon(canvas, *ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_STATUSBAR_FEEDBACK_PRESSED));
}

void FeedbackMenuButton::DrawIcon(gfx::Canvas* canvas) {
  DrawFeedbackIcon(canvas, *ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_STATUSBAR_FEEDBACK));
}

void FeedbackMenuButton::DrawFeedbackIcon(gfx::Canvas* canvas, SkBitmap icon) {
  // Draw the battery icon 5 pixels down to center it.
  static const int kIconVerticalPadding = 5;
  canvas->DrawBitmapInt(icon, 0, kIconVerticalPadding);
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, views::ViewMenuDelegate implementation:

void FeedbackMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  DCHECK(host_);
  host_->ExecuteBrowserCommand(IDC_REPORT_BUG);
}

}  // namespace chromeos
