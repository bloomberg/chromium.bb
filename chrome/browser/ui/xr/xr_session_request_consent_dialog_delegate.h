// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_XR_XR_SESSION_REQUEST_CONSENT_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_XR_XR_SESSION_REQUEST_CONSENT_DIALOG_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

namespace content {
class WebContents;
}

namespace vr {

// Delegate to return appropriate strings for UI elements, and handle user's
// responses from a TabModalConfirmDialog.
class XrSessionRequestConsentDialogDelegate
    : public TabModalConfirmDialogDelegate {
 public:
  XrSessionRequestConsentDialogDelegate(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> response_callback);
  ~XrSessionRequestConsentDialogDelegate() override;

  // TabModalConfirmDialogDelegate:
  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;
  base::string16 GetAcceptButtonTitle() override;
  base::string16 GetCancelButtonTitle() override;

  base::Optional<int> GetDefaultDialogButton() override;
  base::Optional<int> GetInitiallyFocusedButton() override;

  // Metrics helpers
  void OnShowDialog();

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Adding a value at the end is okay.
  enum class ConsentDialogAction : int {
    // The user gave permission to enter an immersive presentation.
    kUserAllowed = 0,
    // The user denied permission to enter an immersive presentation.
    kUserDenied = 1,
    // The user aborted the consent flow by clicking on the permission
    // dialog's 'X' system button.
    kUserAbortedConsentFlow = 2,
    // To insert a new enum, assign the next numeric value to it, and replace
    // the value of of this enum with the value of the added enum.
    kMaxValue = kUserAbortedConsentFlow,
  };

  // TabModalConfirmDialogDelegate:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  // Metrics helpers
  void LogUserAction(ConsentDialogAction action);
  void LogConsentFlowDurationWhenConsentGranted();
  void LogConsentFlowDurationWhenConsentNotGranted();
  void LogConsentFlowDurationWhenUserAborted();

  base::OnceCallback<void(bool)> response_callback_;

  // Metrics related
  base::TimeTicks dialog_presented_at_;

  DISALLOW_COPY_AND_ASSIGN(XrSessionRequestConsentDialogDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_UI_XR_XR_SESSION_REQUEST_CONSENT_DIALOG_DELEGATE_H_
