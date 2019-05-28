// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/xr/xr_session_request_consent_dialog_delegate.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

namespace vr {

XrSessionRequestConsentDialogDelegate::XrSessionRequestConsentDialogDelegate(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> response_callback)
    : TabModalConfirmDialogDelegate(web_contents),
      response_callback_(std::move(response_callback)) {}

XrSessionRequestConsentDialogDelegate::
    ~XrSessionRequestConsentDialogDelegate() = default;

base::string16 XrSessionRequestConsentDialogDelegate::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_XR_CONSENT_DIALOG_TITLE);
}

base::string16 XrSessionRequestConsentDialogDelegate::GetDialogMessage() {
  return l10n_util::GetStringUTF16(IDS_XR_CONSENT_DIALOG_DESCRIPTION);
}

base::string16 XrSessionRequestConsentDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_XR_CONSENT_DIALOG_BUTTON_ALLOW_AND_ENTER_VR);
}

base::string16 XrSessionRequestConsentDialogDelegate::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_XR_CONSENT_DIALOG_BUTTON_DENY_VR);
}

void XrSessionRequestConsentDialogDelegate::OnAccepted() {
  std::move(response_callback_).Run(true);
  LogUserAction(ConsentDialogAction::kUserAllowed);
  LogConsentFlowDurationWhenConsentGranted();
}

void XrSessionRequestConsentDialogDelegate::OnCanceled() {
  std::move(response_callback_).Run(false);
  LogUserAction(ConsentDialogAction::kUserDenied);
  LogConsentFlowDurationWhenConsentNotGranted();
}

void XrSessionRequestConsentDialogDelegate::OnClosed() {
  std::move(response_callback_).Run(false);
  LogUserAction(ConsentDialogAction::kUserAbortedConsentFlow);
  LogConsentFlowDurationWhenUserAborted();
}

void XrSessionRequestConsentDialogDelegate::OnShowDialog() {
  dialog_presented_at_ = base::TimeTicks::Now();
}

void XrSessionRequestConsentDialogDelegate::LogUserAction(
    ConsentDialogAction action) {
  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ConsentFlow", action);
}

void XrSessionRequestConsentDialogDelegate::
    LogConsentFlowDurationWhenConsentGranted() {
  UMA_HISTOGRAM_MEDIUM_TIMES("XR.WebXR.ConsentFlowDuration.ConsentGranted",
                             base::TimeTicks::Now() - dialog_presented_at_);
}

void XrSessionRequestConsentDialogDelegate::
    LogConsentFlowDurationWhenConsentNotGranted() {
  UMA_HISTOGRAM_MEDIUM_TIMES("XR.WebXR.ConsentFlowDuration.ConsentNotGranted",
                             base::TimeTicks::Now() - dialog_presented_at_);
}

void XrSessionRequestConsentDialogDelegate::
    LogConsentFlowDurationWhenUserAborted() {
  UMA_HISTOGRAM_MEDIUM_TIMES("XR.WebXR.ConsentFlowDuration.ConsentFlowAborted",
                             base::TimeTicks::Now() - dialog_presented_at_);
}

base::Optional<int>
XrSessionRequestConsentDialogDelegate::GetDefaultDialogButton() {
  return ui::DIALOG_BUTTON_OK;
}

base::Optional<int>
XrSessionRequestConsentDialogDelegate::GetInitiallyFocusedButton() {
  return ui::DIALOG_BUTTON_CANCEL;
}

}  // namespace vr
