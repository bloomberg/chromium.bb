// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_session_request_consent_manager_impl.h"

#include <utility>

#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/xr/xr_session_request_consent_dialog_delegate.h"
#include "content/public/browser/web_contents.h"

namespace vr {

XRSessionRequestConsentManagerImpl::XRSessionRequestConsentManagerImpl() =
    default;

XRSessionRequestConsentManagerImpl::~XRSessionRequestConsentManagerImpl() =
    default;

void XRSessionRequestConsentManagerImpl::ShowDialogAndGetConsent(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> response_callback) {
  TabModalConfirmDialog::Create(new XrSessionRequestConsentDialogDelegate(
                                    web_contents, std::move(response_callback)),
                                web_contents);
}

}  // namespace vr
