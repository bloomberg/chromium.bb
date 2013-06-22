// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_driver_impl.h"

#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_delegate.h"
#include "components/autofill/core/common/autofill_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "ipc/ipc_message_macros.h"

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
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(kAutofillDriverImplWebContentsUserDataKey,
                        new AutofillDriverImpl(contents,
                                               delegate,
                                               app_locale,
                                               enable_download_manager));
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
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : content::WebContentsObserver(web_contents),
      autofill_manager_(new AutofillManager(
          this, delegate, app_locale, enable_download_manager)) {
  SetAutofillExternalDelegate(scoped_ptr<AutofillExternalDelegate>(
      new AutofillExternalDelegate(web_contents, autofill_manager_.get())));
}

AutofillDriverImpl::~AutofillDriverImpl() {}

content::WebContents* AutofillDriverImpl::GetWebContents() {
  return web_contents();
}

bool AutofillDriverImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillDriverImpl, message)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_FormsSeen, autofill_manager_.get(),
                        AutofillManager::OnFormsSeen)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_FormSubmitted, autofill_manager_.get(),
                        AutofillManager::OnFormSubmitted)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_TextFieldDidChange,
                        autofill_manager_.get(),
                        AutofillManager::OnTextFieldDidChange)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_QueryFormFieldAutofill,
                        autofill_manager_.get(),
                        AutofillManager::OnQueryFormFieldAutofill)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowAutofillDialog,
                        autofill_manager_.get(),
                        AutofillManager::OnShowAutofillDialog)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_FillAutofillFormData,
                        autofill_manager_.get(),
                        AutofillManager::OnFillAutofillFormData)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidPreviewAutofillFormData,
                        autofill_manager_.get(),
                        AutofillManager::OnDidPreviewAutofillFormData)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidFillAutofillFormData,
                        autofill_manager_.get(),
                        AutofillManager::OnDidFillAutofillFormData)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidShowAutofillSuggestions,
                        autofill_manager_.get(),
                        AutofillManager::OnDidShowAutofillSuggestions)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_DidEndTextFieldEditing,
                        autofill_manager_.get(),
                        AutofillManager::OnDidEndTextFieldEditing)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_HideAutofillUi, autofill_manager_.get(),
                        AutofillManager::OnHideAutofillUi)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_AddPasswordFormMapping,
                        autofill_manager_.get(),
                        AutofillManager::OnAddPasswordFormMapping)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowPasswordSuggestions,
                        autofill_manager_.get(),
                        AutofillManager::OnShowPasswordSuggestions)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_SetDataList, autofill_manager_.get(),
                        AutofillManager::OnSetDataList)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_RequestAutocomplete,
                        autofill_manager_.get(),
                        AutofillManager::OnRequestAutocomplete)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_ClickFailed, autofill_manager_.get(),
                        AutofillManager::OnClickFailed)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_MaybeShowAutocheckoutBubble,
                        autofill_manager_.get(),
                        AutofillManager::OnMaybeShowAutocheckoutBubble)
    IPC_MESSAGE_FORWARD(AutofillHostMsg_RemoveAutocompleteEntry,
                        autofill_manager_.get(),
                        AutofillManager::RemoveAutocompleteEntry)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillDriverImpl::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_navigation_to_different_page())
    autofill_manager_->Reset();
}

void AutofillDriverImpl::SetAutofillExternalDelegate(
    scoped_ptr<AutofillExternalDelegate> delegate) {
  autofill_external_delegate_ = delegate.Pass();
  autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
}

void AutofillDriverImpl::SetAutofillManager(
    scoped_ptr<AutofillManager> manager) {
  autofill_manager_ = manager.Pass();
  autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
}

}  // namespace autofill
