// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/ssl_status.h"
#include "ipc/ipc_message_macros.h"
#include "net/cert/cert_status_flags.h"

namespace password_manager {

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::WebContents* web_contents,
    PasswordManagerClient* client,
    autofill::AutofillClient* autofill_client)
    : WebContentsObserver(web_contents),
      password_manager_(client),
      password_generation_manager_(client),
      password_autofill_manager_(client, autofill_client) {
  DCHECK(web_contents);
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() {}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  DCHECK(web_contents());
  web_contents()->GetRenderViewHost()->Send(new AutofillMsg_FillPasswordForm(
      web_contents()->GetRenderViewHost()->GetRoutingID(), form_data));
}

void ContentPasswordManagerDriver::AllowPasswordGenerationForForm(
    const autofill::PasswordForm& form) {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_FormNotBlacklisted(host->GetRoutingID(), form));
}

void ContentPasswordManagerDriver::AccountCreationFormsFound(
    const std::vector<autofill::FormData>& forms) {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_AccountCreationFormsDetected(host->GetRoutingID(),
                                                          forms));
}

void ContentPasswordManagerDriver::FillSuggestion(
    const base::string16& username,
    const base::string16& password) {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(
      new AutofillMsg_FillPasswordSuggestion(host->GetRoutingID(),
                                             username,
                                             password));
}

void ContentPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(
      new AutofillMsg_PreviewPasswordSuggestion(host->GetRoutingID(),
                                                username,
                                                password));
}

void ContentPasswordManagerDriver::ClearPreviewedForm() {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(
      new AutofillMsg_ClearPreviewedForm(host->GetRoutingID()));
}

bool ContentPasswordManagerDriver::DidLastPageLoadEncounterSSLErrors() {
  DCHECK(web_contents());
  // TODO(vabr): This is a wrong entry to look at for HTTP basic auth,
  // http://crbug.com/388246.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }

  return net::IsCertStatusError(entry->GetSSL().cert_status);
}

bool ContentPasswordManagerDriver::IsOffTheRecord() {
  DCHECK(web_contents());
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

PasswordGenerationManager*
ContentPasswordManagerDriver::GetPasswordGenerationManager() {
  return &password_generation_manager_;
}

PasswordManager* ContentPasswordManagerDriver::GetPasswordManager() {
  return &password_manager_;
}

PasswordAutofillManager*
ContentPasswordManagerDriver::GetPasswordAutofillManager() {
  return &password_autofill_manager_;
}

void ContentPasswordManagerDriver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  password_manager_.DidNavigateMainFrame(details.is_in_page);
}

bool ContentPasswordManagerDriver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordManager, message)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PasswordFormsParsed,
                      &password_manager_,
                      PasswordManager::OnPasswordFormsParsed)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PasswordFormsRendered,
                      &password_manager_,
                      PasswordManager::OnPasswordFormsRendered)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PasswordFormSubmitted,
                      &password_manager_,
                      PasswordManager::OnPasswordFormSubmitted)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowPasswordSuggestions,
                      &password_autofill_manager_,
                      PasswordAutofillManager::OnShowPasswordSuggestions)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_AddPasswordFormMapping,
                      &password_autofill_manager_,
                      PasswordAutofillManager::OnAddPasswordFormMapping)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_RecordSavePasswordProgress,
                      password_manager_.client(),
                      PasswordManagerClient::LogSavePasswordProgress)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

autofill::AutofillManager* ContentPasswordManagerDriver::GetAutofillManager() {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::FromWebContents(web_contents());
  return driver ? driver->autofill_manager() : NULL;
}

}  // namespace password_manager
