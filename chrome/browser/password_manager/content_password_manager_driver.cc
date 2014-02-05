// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/content_password_manager_driver.h"

#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/ssl_status.h"
#include "net/cert/cert_status_flags.h"

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::WebContents* web_contents,
    PasswordManagerDelegate* delegate)
    : WebContentsObserver(web_contents),
      password_manager_(web_contents, delegate),
      password_generation_manager_(web_contents, delegate) {
  DCHECK(web_contents);
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() {}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  DCHECK(web_contents());
  web_contents()->GetRenderViewHost()->Send(new AutofillMsg_FillPasswordForm(
      web_contents()->GetRenderViewHost()->GetRoutingID(), form_data));
}

bool ContentPasswordManagerDriver::DidLastPageLoadEncounterSSLErrors() {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
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

void ContentPasswordManagerDriver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  password_manager_.DidNavigateMainFrame(details.is_in_page);
}

bool ContentPasswordManagerDriver::OnMessageReceived(
    const IPC::Message& message) {
  return password_manager_.OnMessageReceived(message);
}
