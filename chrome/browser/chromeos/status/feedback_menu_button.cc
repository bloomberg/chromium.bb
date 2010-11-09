// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/feedback_menu_button.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// FeedbackMenuButton

FeedbackMenuButton::FeedbackMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      host_(host) {
  DCHECK(host_);
  SetTooltipText(l10n_util::GetString(IDS_STATUSBAR_FEEDBACK_TOOLTIP));
  SetIcon(*ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_STATUSBAR_FEEDBACK));
}

FeedbackMenuButton::~FeedbackMenuButton() {
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, views::ViewMenuDelegate implementation:

void FeedbackMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  DCHECK(host_);
  host_->ExecuteBrowserCommand(IDC_REPORT_BUG);
}

}  // namespace chromeos
