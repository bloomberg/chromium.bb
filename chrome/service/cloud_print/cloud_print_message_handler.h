// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_MESSAGE_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_ipc_server.h"

namespace base {
class DictionaryValue;
}

namespace IPC {
class Message;
class Sender;
}

namespace cloud_print {

// Handles IPC messages for Cloud Print. Lives on the main thread.
class CloudPrintMessageHandler : public ServiceIPCServer::MessageHandler {
 public:
  CloudPrintMessageHandler(IPC::Sender* ipc_sender,
                           CloudPrintProxy::Provider* proxy_provider);
  ~CloudPrintMessageHandler() override;

  // ServiceIPCServer::MessageHandler implementation.
  bool HandleMessage(const IPC::Message& message) override;

 private:
  // IPC message handlers.
  void OnEnableCloudPrintProxyWithRobot(
      const std::string& robot_auth_code,
      const std::string& robot_email,
      const std::string& user_email,
      const base::DictionaryValue& user_settings);
  void OnGetCloudPrintProxyInfo();
  void OnGetPrinters();
  void OnDisableCloudPrintProxy();

  IPC::Sender* ipc_sender_;  // Owned by our owner.
  CloudPrintProxy::Provider* proxy_provider_;  // Owned by our owner.

  DISALLOW_COPY_AND_ASSIGN(CloudPrintMessageHandler);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_MESSAGE_HANDLER_H_
