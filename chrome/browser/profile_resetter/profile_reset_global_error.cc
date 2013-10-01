// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_reset_global_error.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/show_profile_reset_bubble.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ProfileResetGlobalError::ProfileResetGlobalError() {
}

ProfileResetGlobalError::~ProfileResetGlobalError() {
}

bool ProfileResetGlobalError::HasMenuItem() {
  return true;
}

int ProfileResetGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SETTINGS_RESET_BUBBLE;
}

string16 ProfileResetGlobalError::MenuItemLabel() {
  return l10n_util::GetStringFUTF16(IDS_RESET_SETTINGS_MENU_ITEM,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

void ProfileResetGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowProfileResetBubble(browser, reset_callback_);
}

// We don't use the GlobalErrorBubbleViewBase since it is not flexible enough
// for our needs.
bool ProfileResetGlobalError::HasBubbleView() {
  return false;
}

string16 ProfileResetGlobalError::GetBubbleViewTitle() {
  NOTREACHED();
  return string16();
}

std::vector<string16> ProfileResetGlobalError::GetBubbleViewMessages() {
  NOTREACHED();
  return std::vector<string16>();
}

string16 ProfileResetGlobalError::GetBubbleViewAcceptButtonLabel() {
  NOTREACHED();
  return string16();
}

string16 ProfileResetGlobalError::GetBubbleViewCancelButtonLabel() {
  NOTREACHED();
  return string16();
}

void ProfileResetGlobalError::OnBubbleViewDidClose(Browser* browser) {
  NOTREACHED();
}

void ProfileResetGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  NOTREACHED();
}

void ProfileResetGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  NOTREACHED();
}
