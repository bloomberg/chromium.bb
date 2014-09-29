// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_credential_manager_dispatcher.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace password_manager {

ContentCredentialManagerDispatcher::ContentCredentialManagerDispatcher(
    content::WebContents* web_contents,
    PasswordManagerClient* client)
    : WebContentsObserver(web_contents),
      client_(client) {
  DCHECK(web_contents);
}

ContentCredentialManagerDispatcher::~ContentCredentialManagerDispatcher() {}

bool ContentCredentialManagerDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentCredentialManagerDispatcher, message)
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_NotifyFailedSignIn,
                        OnNotifyFailedSignIn);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_NotifySignedIn,
                        OnNotifySignedIn);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_NotifySignedOut,
                        OnNotifySignedOut);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_RequestCredential,
                        OnRequestCredential);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentCredentialManagerDispatcher::OnNotifyFailedSignIn(
    int request_id,
    const password_manager::CredentialInfo&) {
  // TODO(mkwst): This is a stub.
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeFailedSignIn(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));
}

void ContentCredentialManagerDispatcher::OnNotifySignedIn(
    int request_id,
    const password_manager::CredentialInfo&) {
  // TODO(mkwst): This is a stub.
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeSignedIn(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));
}

void ContentCredentialManagerDispatcher::OnNotifySignedOut(int request_id) {
  // TODO(mkwst): This is a stub.
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeSignedOut(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));
}

void ContentCredentialManagerDispatcher::OnRequestCredential(
    int request_id,
    bool zero_click_only,
    const std::vector<GURL>& federations) {
  // TODO(mkwst): This is a stub.
  password_manager::CredentialInfo info(base::ASCIIToUTF16("id"),
                                        base::ASCIIToUTF16("name"),
                                        GURL("https://example.com/image.png"));
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_SendCredential(
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          request_id,
          info));
}

}  // namespace password_manager
