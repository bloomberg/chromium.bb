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

 private:
  // TabModalConfirmDialogDelegate:
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  base::OnceCallback<void(bool)> response_callback_;

  DISALLOW_COPY_AND_ASSIGN(XrSessionRequestConsentDialogDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_UI_XR_XR_SESSION_REQUEST_CONSENT_DIALOG_DELEGATE_H_
