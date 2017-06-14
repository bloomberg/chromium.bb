// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_error_dialog.h"

#include "base/base_switches.h"
#include "base/bind.h"
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

bool g_is_showing_profile_error_dialog = false;

void OnProfileErrorDialogDismissed(const std::string& diagnostics,
                                   bool needs_feedback) {
  g_is_showing_profile_error_dialog = false;

  if (!needs_feedback)
    return;

  std::string feedback_description =
      l10n_util::GetStringUTF8(IDS_PROFILE_ERROR_FEEDBACK_DESCRIPTION);
  if (!diagnostics.empty()) {
    // TODO(afakhry): Add support to inject diagnostics to the feedback
    // reports without adding them to the description. crbug.com/708511.
    feedback_description +=
        "\n\n" +
        l10n_util::GetStringUTF8(IDS_PROFILE_ERROR_FEEDBACK_DIAGNOSTICS_LINE) +
        diagnostics;
  }

  chrome::ShowFeedbackPage(nullptr, chrome::kFeedbackSourceProfileErrorDialog,
                           feedback_description, kProfileErrorFeedbackCategory);
}
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

  if (g_is_showing_profile_error_dialog)
    return;

  g_is_showing_profile_error_dialog = true;
  chrome::ShowWarningMessageBoxWithCheckbox(
      nullptr, l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_TITLE),
      l10n_util::GetStringUTF16(message_id),
      l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_CHECKBOX),
      base::Bind(&OnProfileErrorDialogDismissed, diagnostics));
#endif
}
