// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/content_password_manager_driver.h"

#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/ssl_status.h"
#include "net/cert/cert_status_flags.h"

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() {
}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  web_contents_->GetRenderViewHost()->Send(
      new AutofillMsg_FillPasswordForm(
          web_contents_->GetRenderViewHost()->GetRoutingID(),
          form_data));
}

bool ContentPasswordManagerDriver::DidLastPageLoadEncounterSSLErrors() {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  return net::IsCertStatusError(entry->GetSSL().cert_status);
}
