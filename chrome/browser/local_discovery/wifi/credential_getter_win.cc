// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/credential_getter_win.h"

#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

namespace local_discovery {

namespace wifi {

CredentialGetterWin::CredentialGetterWin() {
}

CredentialGetterWin::~CredentialGetterWin() {
}

void CredentialGetterWin::StartGetCredentials(
    const std::string& network_guid,
    const CredentialsCallback& callback) {
  callback_ = callback;
  callback_runner_ = base::MessageLoopProxy::current();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CredentialGetterWin::StartOnIOThread, this, network_guid));
}

void CredentialGetterWin::StartOnIOThread(const std::string& network_guid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::UtilityProcessHost* host = content::UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current());
  host->ElevatePrivileges();
  host->Send(new ChromeUtilityHostMsg_GetWiFiCredentials(network_guid));
}

bool CredentialGetterWin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialGetterWin, message)
  IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotWiFiCredentials, OnGotCredentials)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialGetterWin::OnProcessCrashed(int exit_code) {
  PostCallback(false, "");
}

void CredentialGetterWin::OnProcessLaunchFailed() {
  PostCallback(false, "");
}

void CredentialGetterWin::OnGotCredentials(const std::string& key_data,
                                           bool success) {
  PostCallback(success, key_data);
}

void CredentialGetterWin::PostCallback(bool success,
                                       const std::string& key_data) {
  callback_runner_->PostTask(FROM_HERE,
                             base::Bind(callback_, success, key_data));
}

}  // namespace wifi
}  // namespace local_discovery
