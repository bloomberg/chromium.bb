// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;
using extensions::NetworkingPrivateCredentialsGetter;

namespace {

class CredentialsGetterHostClient : public content::UtilityProcessHostClient {
 public:
  explicit CredentialsGetterHostClient(const std::string& public_key);

  virtual ~CredentialsGetterHostClient();

  // UtilityProcessHostClient
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual void OnProcessLaunchFailed() OVERRIDE;

  // IPC message handlers.
  void OnGotCredentials(const std::string& key_data, bool success);

  // Starts the utility process that gets wifi passphrase from system.
  void StartProcessOnIOThread(
      const std::string& network_guid,
      const extensions::NetworkingPrivateServiceClient::CryptoVerify::
          VerifyAndEncryptCredentialsCallback& callback);

 private:
  // Public key used to encrypt results
  std::vector<uint8> public_key_;

  // Callback for reporting the result.
  extensions::NetworkingPrivateServiceClient::CryptoVerify::
      VerifyAndEncryptCredentialsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsGetterHostClient);
};

CredentialsGetterHostClient::CredentialsGetterHostClient(
    const std::string& public_key)
    : public_key_(public_key.begin(), public_key.end()) {
}

CredentialsGetterHostClient::~CredentialsGetterHostClient() {}

bool CredentialsGetterHostClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialsGetterHostClient, message)
  IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotWiFiCredentials, OnGotCredentials)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialsGetterHostClient::OnProcessCrashed(int exit_code) {
  callback_.Run("", "Process Crashed");
}

void CredentialsGetterHostClient::OnProcessLaunchFailed() {
  callback_.Run("", "Process Launch Failed");
}

void CredentialsGetterHostClient::OnGotCredentials(const std::string& key_data,
                                                   bool success) {
  if (success) {
    std::vector<uint8> ciphertext;
    if (!networking_private_crypto::EncryptByteString(
            public_key_, key_data, &ciphertext)) {
      callback_.Run("", "Encrypt Credentials Failed");
      return;
    }

    std::string base64_encoded_key_data;
    base::Base64Encode(std::string(ciphertext.begin(), ciphertext.end()),
                       &base64_encoded_key_data);
    callback_.Run(base64_encoded_key_data, "");
  } else {
    callback_.Run("", "Get Credentials Failed");
  }
}

void CredentialsGetterHostClient::StartProcessOnIOThread(
    const std::string& network_guid,
    const extensions::NetworkingPrivateServiceClient::CryptoVerify::
        VerifyAndEncryptCredentialsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, base::MessageLoopProxy::current());
  callback_ = callback;
  host->ElevatePrivileges();
  host->Send(new ChromeUtilityHostMsg_GetWiFiCredentials(network_guid));
}

}  // namespace

namespace extensions {

class NetworkingPrivateCredentialsGetterWin
    : public NetworkingPrivateCredentialsGetter {
 public:
  NetworkingPrivateCredentialsGetterWin();

  virtual void Start(
      const std::string& network_guid,
      const std::string& public_key,
      const extensions::NetworkingPrivateServiceClient::CryptoVerify::
          VerifyAndEncryptCredentialsCallback& callback) OVERRIDE;

 private:
  virtual ~NetworkingPrivateCredentialsGetterWin();

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateCredentialsGetterWin);
};

NetworkingPrivateCredentialsGetterWin::NetworkingPrivateCredentialsGetterWin() {
}

void NetworkingPrivateCredentialsGetterWin::Start(
    const std::string& network_guid,
    const std::string& public_key,
    const extensions::NetworkingPrivateServiceClient::CryptoVerify::
        VerifyAndEncryptCredentialsCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CredentialsGetterHostClient::StartProcessOnIOThread,
                 new CredentialsGetterHostClient(public_key),
                 network_guid,
                 callback));
}

NetworkingPrivateCredentialsGetterWin::
    ~NetworkingPrivateCredentialsGetterWin() {}

NetworkingPrivateCredentialsGetter*
NetworkingPrivateCredentialsGetter::Create() {
  return new NetworkingPrivateCredentialsGetterWin();
}

}  // namespace extensions
