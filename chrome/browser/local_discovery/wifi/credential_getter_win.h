// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_CREDENTIAL_GETTER_WIN_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_CREDENTIAL_GETTER_WIN_H_

#include <string>

#include "base/single_thread_task_runner.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace local_discovery {
namespace wifi {

class CredentialGetterWin : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(bool success, const std::string& key)>
      CredentialsCallback;

  CredentialGetterWin();

  void StartGetCredentials(const std::string& network_guid,
                           const CredentialsCallback& callback);

 private:
  ~CredentialGetterWin() override;

  // UtilityProcessHostClient
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed() override;

  // IPC message handlers.
  void OnGotCredentials(const std::string& key_data, bool success);

  void StartOnIOThread(const std::string& network_guid);

  void PostCallback(bool success, const std::string& key_data);

  CredentialsCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;
};

}  // namespace wifi
}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_CREDENTIAL_GETTER_WIN_H_
