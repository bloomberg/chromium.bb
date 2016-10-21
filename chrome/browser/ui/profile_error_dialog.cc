// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_error_dialog.h"

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

#if !defined(OS_ANDROID)
constexpr char kProfileErrorFeedbackCategory[] = "FEEDBACK_PROFILE_ERROR";
#endif  // !defined(OS_ANDROID)

}  // namespace

void ShowProfileErrorDialog(ProfileErrorType type,
                            int message_id,
                            const std::string& diagnostics) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  UMA_HISTOGRAM_ENUMERATION("Profile.ProfileError", static_cast<int>(type),
                            static_cast<int>(ProfileErrorType::END));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoErrorDialogs)) {
    return;
  }

  static bool is_showing_profile_error_dialog = false;
  if (is_showing_profile_error_dialog)
    return;

  base::AutoReset<bool> resetter(&is_showing_profile_error_dialog, true);
  if (chrome::ShowWarningMessageBoxWithCheckbox(
          nullptr, l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_TITLE),
          l10n_util::GetStringUTF16(message_id),
          l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_CHECKBOX))) {
    std::string feedback_description =
        l10n_util::GetStringUTF8(IDS_PROFILE_ERROR_FEEDBACK_DESCRIPTION);
    feedback_description += "\n" + diagnostics;

    chrome::ShowFeedbackPage(nullptr, feedback_description,
                             kProfileErrorFeedbackCategory);
  }
#endif
}
