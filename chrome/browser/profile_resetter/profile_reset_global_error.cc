// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_reset_global_error.h"

#include "base/metrics/histogram.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/profile_reset_bubble.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The maximum number of ignored bubbles we track in the NumNoThanksPerReset
// histogram.
const int kMaxIgnored = 50;

// The number of buckets we want the NumNoThanksPerReset histogram to use.
const int kNumIgnoredBuckets = 5;

}  // namespace


// ProfileResetGlobalError ---------------------------------------------------

ProfileResetGlobalError::ProfileResetGlobalError(Profile* profile)
    : profile_(profile), num_times_bubble_view_shown_(0), bubble_view_(NULL) {}

ProfileResetGlobalError::~ProfileResetGlobalError() {}

bool ProfileResetGlobalError::HasMenuItem() { return true; }

int ProfileResetGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SETTINGS_RESET_BUBBLE;
}

string16 ProfileResetGlobalError::MenuItemLabel() {
  return l10n_util::GetStringFUTF16(
      IDS_RESET_SETTINGS_MENU_ITEM,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

void ProfileResetGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

bool ProfileResetGlobalError::HasBubbleView() { return true; }

bool ProfileResetGlobalError::HasShownBubbleView() {
  return num_times_bubble_view_shown_ > 0;
}

void ProfileResetGlobalError::ShowBubbleView(Browser* browser) {
  if (bubble_view_)
    return;
  ++num_times_bubble_view_shown_;
  bubble_view_ = ShowProfileResetBubble(AsWeakPtr(), browser);
}

void ProfileResetGlobalError::OnBubbleViewDidClose() {
  bubble_view_ = NULL;
}

void ProfileResetGlobalError::OnBubbleViewResetButtonPressed(
    bool send_feedback) {
  // TODO(engedy): Integrate with the AutomaticProfileResetter.
  UMA_HISTOGRAM_CUSTOM_COUNTS("SettingsResetBubble.NumNoThanksPerReset",
                              num_times_bubble_view_shown_ - 1,
                              0,
                              kMaxIgnored,
                              kNumIgnoredBuckets);
}

void ProfileResetGlobalError::OnBubbleViewNoThanksButtonPressed() {
  // TODO(engedy): Integrate with the AutomaticProfileResetter.
}

GlobalErrorBubbleViewBase* ProfileResetGlobalError::GetBubbleView() {
  return bubble_view_;
}
