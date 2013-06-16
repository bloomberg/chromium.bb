// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_driver_impl.h"

#include "components/autofill/browser/autofill_external_delegate.h"
#include "components/autofill/browser/autofill_manager.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

namespace {

const char kAutofillDriverImplWebContentsUserDataKey[] =
    "web_contents_autofill_driver_impl";

}  // namespace

// static
void AutofillDriverImpl::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    autofill::AutofillManagerDelegate* delegate,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager,
    bool enable_native_ui) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(kAutofillDriverImplWebContentsUserDataKey,
                        new AutofillDriverImpl(contents,
                                               delegate,
                                               app_locale,
                                               enable_download_manager,
                                               enable_native_ui));
  // Trigger the lazy creation of AutocheckoutWhitelistManagerService, and
  // schedule a fetch of the Autocheckout whitelist file if it's not already
  // loaded. This helps ensure that the whitelist will be available by the time
  // the user navigates to a form on which Autocheckout should be enabled.
  delegate->GetAutocheckoutWhitelistManager();
}

// static
AutofillDriverImpl* AutofillDriverImpl::FromWebContents(
    content::WebContents* contents) {
  return static_cast<AutofillDriverImpl*>(
      contents->GetUserData(kAutofillDriverImplWebContentsUserDataKey));
}

AutofillDriverImpl::AutofillDriverImpl(
    content::WebContents* web_contents,
    autofill::AutofillManagerDelegate* delegate,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager,
    bool enable_native_ui)
    : content::WebContentsObserver(web_contents),
      autofill_manager_(this, delegate, app_locale, enable_download_manager) {
  if (enable_native_ui) {
    // TODO(blundell): Eliminate AutofillExternalDelegate being a WCUD and
    // transfer ownership of it to this class.
    AutofillExternalDelegate::CreateForWebContentsAndManager(
        web_contents, &autofill_manager_);
    autofill_manager_.SetExternalDelegate(
        AutofillExternalDelegate::FromWebContents(web_contents));
  }
}

AutofillDriverImpl::~AutofillDriverImpl() {}

content::WebContents* AutofillDriverImpl::GetWebContents() {
  return web_contents();
}

bool AutofillDriverImpl::OnMessageReceived(const IPC::Message& message) {
  // TODO(blundell): Move IPC handling into this class.
  return autofill_manager_.OnMessageReceived(message);
}

void AutofillDriverImpl::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // TODO(blundell): Move the logic of this method into this class.
  autofill_manager_.DidNavigateMainFrame(details, params);
}

}  // namespace autofill
