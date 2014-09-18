// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_reset_global_error.h"

#include "base/metrics/histogram.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/profile_reset_bubble.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

base::TimeDelta GetPromptDelayHistogramMaximum() {
  return base::TimeDelta::FromDays(7);
}

// Records the delay between when the reset prompt is triggered and when the
// bubble can actually be shown.
void RecordPromptDelay(const base::TimeDelta& delay) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "AutomaticProfileReset.PromptDelay", delay,
      base::TimeDelta::FromSeconds(1), GetPromptDelayHistogramMaximum(), 50);
}

}  // namespace


// ProfileResetGlobalError ---------------------------------------------------

ProfileResetGlobalError::ProfileResetGlobalError(Profile* profile)
    : profile_(profile), has_shown_bubble_view_(false), bubble_view_(NULL) {
  AutomaticProfileResetter* automatic_profile_resetter =
      AutomaticProfileResetterFactory::GetForBrowserContext(profile_);
  if (automatic_profile_resetter)
    automatic_profile_resetter_ = automatic_profile_resetter->AsWeakPtr();
}

ProfileResetGlobalError::~ProfileResetGlobalError() {
  if (!has_shown_bubble_view_)
    RecordPromptDelay(GetPromptDelayHistogramMaximum());
}

// static
bool ProfileResetGlobalError::IsSupportedOnPlatform() {
  return IsProfileResetBubbleSupported();
}

bool ProfileResetGlobalError::HasMenuItem() { return true; }

int ProfileResetGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SETTINGS_RESET_BUBBLE;
}

base::string16 ProfileResetGlobalError::MenuItemLabel() {
  return l10n_util::GetStringFUTF16(
      IDS_RESET_SETTINGS_MENU_ITEM,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

void ProfileResetGlobalError::ExecuteMenuItem(Browser* browser) {
  chrome::ShowSettingsSubPage(browser, chrome::kResetProfileSettingsSubPage);
}

bool ProfileResetGlobalError::HasBubbleView() { return true; }

bool ProfileResetGlobalError::HasShownBubbleView() {
  return has_shown_bubble_view_;
}

void ProfileResetGlobalError::ShowBubbleView(Browser* browser) {
  if (has_shown_bubble_view_)
    return;

  has_shown_bubble_view_ = true;
  bubble_view_ = ShowProfileResetBubble(AsWeakPtr(), browser);

  if (automatic_profile_resetter_)
    automatic_profile_resetter_->NotifyDidShowResetBubble();
  RecordPromptDelay(timer_.Elapsed());
}

void ProfileResetGlobalError::OnBubbleViewDidClose() {
  bubble_view_ = NULL;
}

void ProfileResetGlobalError::OnBubbleViewResetButtonPressed(
    bool send_feedback) {
  if (automatic_profile_resetter_)
    automatic_profile_resetter_->TriggerProfileReset(send_feedback);
}

void ProfileResetGlobalError::OnBubbleViewNoThanksButtonPressed() {
  if (automatic_profile_resetter_)
    automatic_profile_resetter_->SkipProfileReset();
}

GlobalErrorBubbleViewBase* ProfileResetGlobalError::GetBubbleView() {
  return bubble_view_;
}
