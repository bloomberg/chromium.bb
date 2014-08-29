// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_CREDENTIAL_GETTER_WIN_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_CREDENTIAL_GETTER_WIN_H_

#include <string>

#include "base/message_loop/message_loop_proxy.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace local_discovery {
namespace wifi {

class CredentialGetterWin : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(bool success, const std::string& key)>
      CredentialsCallback;

  CredentialGetterWin();
  virtual ~CredentialGetterWin();

  void StartGetCredentials(const std::string& network_guid,
                           const CredentialsCallback& callback);

 private:
  // UtilityProcessHostClient
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual void OnProcessLaunchFailed() OVERRIDE;

  // IPC message handlers.
  void OnGotCredentials(const std::string& key_data, bool success);

  void StartOnIOThread(const std::string& network_guid);

  void PostCallback(bool success, const std::string& key_data);

  CredentialsCallback callback_;
  scoped_refptr<base::MessageLoopProxy> callback_runner_;
};

}  // namespace wifi
}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_CREDENTIAL_GETTER_WIN_H_
