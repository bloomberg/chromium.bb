// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_message_handler.h"

#include <vector>

#include "chrome/common/service_messages.h"
#include "ipc/ipc_sender.h"

namespace cloud_print {

CloudPrintMessageHandler::CloudPrintMessageHandler(
    IPC::Sender* ipc_sender,
    CloudPrintProxy::Provider* proxy_provider)
    : ipc_sender_(ipc_sender), proxy_provider_(proxy_provider) {
  DCHECK(ipc_sender);
  DCHECK(proxy_provider);
}

CloudPrintMessageHandler::~CloudPrintMessageHandler() {
}

bool CloudPrintMessageHandler::HandleMessage(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CloudPrintMessageHandler, message)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxyWithRobot,
                        OnEnableCloudPrintProxyWithRobot)
    IPC_MESSAGE_HANDLER(ServiceMsg_DisableCloudPrintProxy,
                        OnDisableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_GetCloudPrintProxyInfo,
                        OnGetCloudPrintProxyInfo)
    IPC_MESSAGE_HANDLER(ServiceMsg_GetPrinters, OnGetPrinters)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CloudPrintMessageHandler::OnEnableCloudPrintProxyWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email,
    const base::DictionaryValue& user_settings) {
  proxy_provider_->GetCloudPrintProxy()->EnableForUserWithRobot(
      robot_auth_code, robot_email, user_email, user_settings);
}

void CloudPrintMessageHandler::OnGetCloudPrintProxyInfo() {
  CloudPrintProxyInfo info;
  proxy_provider_->GetCloudPrintProxy()->GetProxyInfo(&info);
  ipc_sender_->Send(new ServiceHostMsg_CloudPrintProxy_Info(info));
}

void CloudPrintMessageHandler::OnGetPrinters() {
  std::vector<std::string> printers;
  proxy_provider_->GetCloudPrintProxy()->GetPrinters(&printers);
  ipc_sender_->Send(new ServiceHostMsg_Printers(printers));
}

void CloudPrintMessageHandler::OnDisableCloudPrintProxy() {
  proxy_provider_->GetCloudPrintProxy()->UnregisterPrintersAndDisableForUser();
}

}  // namespace cloud_print
