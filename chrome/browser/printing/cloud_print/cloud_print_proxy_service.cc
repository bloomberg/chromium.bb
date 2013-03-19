// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"

#include <stack>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "printing/backend/print_backend.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

CloudPrintProxyService::CloudPrintProxyService(Profile* profile)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      enforcing_connector_policy_(false) {
}

CloudPrintProxyService::~CloudPrintProxyService() {
}

void CloudPrintProxyService::Initialize() {
  if (profile_->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      (!profile_->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty() ||
       !profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled))) {
    // If the cloud print proxy is enabled, or the policy preventing it from
    // being enabled is set, establish a channel with the service process and
    // update the status. This will check the policy when the status is sent
    // back.
    RefreshStatusFromService();
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCloudPrintProxyEnabled,
      base::Bind(
          base::IgnoreResult(
              &CloudPrintProxyService::ApplyCloudPrintConnectorPolicy),
          base::Unretained(this)));
}

void CloudPrintProxyService::RefreshStatusFromService() {
  InvokeServiceTask(
      base::Bind(&CloudPrintProxyService::RefreshCloudPrintProxyStatus,
                 weak_factory_.GetWeakPtr()));
}

bool CloudPrintProxyService::EnforceCloudPrintConnectorPolicyAndQuit() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enforcing_connector_policy_ = true;
  if (ApplyCloudPrintConnectorPolicy())
    return true;
  return false;
}

void CloudPrintProxyService::EnableForUser(const std::string& lsid,
                                           const std::string& email) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled)) {
    InvokeServiceTask(
        base::Bind(&CloudPrintProxyService::EnableCloudPrintProxy,
                   weak_factory_.GetWeakPtr(), lsid, email));
  }
}

void CloudPrintProxyService::EnableForUserWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email,
    bool connect_new_printers,
    const std::vector<std::string>& printer_blacklist) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled)) {
    InvokeServiceTask(
        base::Bind(&CloudPrintProxyService::EnableCloudPrintProxyWithRobot,
                   weak_factory_.GetWeakPtr(), robot_auth_code, robot_email,
                   user_email, connect_new_printers, printer_blacklist));
  }
}

void CloudPrintProxyService::DisableForUser() {
  InvokeServiceTask(
      base::Bind(&CloudPrintProxyService::DisableCloudPrintProxy,
                 weak_factory_.GetWeakPtr()));
}

bool CloudPrintProxyService::ApplyCloudPrintConnectorPolicy() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled)) {
    std::string email =
        profile_->GetPrefs()->GetString(prefs::kCloudPrintEmail);
    if (!email.empty()) {
      DisableForUser();
      profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
      if (enforcing_connector_policy_) {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&CloudPrintProxyService::RefreshCloudPrintProxyStatus,
                       weak_factory_.GetWeakPtr()));
      }
      return false;
    } else if (enforcing_connector_policy_) {
      MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    }
  }
  return true;
}

void CloudPrintProxyService::OnCloudPrintSetupClosed() {
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(&chrome::EndKeepAlive));
}

void CloudPrintProxyService::GetPrintersAvalibleForRegistration(
      std::vector<std::string>* printers) {
  base::FilePath list_path(
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCloudPrintSetupProxy));
  if (!list_path.empty()) {
    std::string printers_json;
    file_util::ReadFileToString(list_path, &printers_json);
    scoped_ptr<Value> value(base::JSONReader::Read(printers_json));
    base::ListValue* list = NULL;
    if (value && value->GetAsList(&list) && list) {
      for (size_t i = 0; i < list->GetSize(); ++i) {
        std::string printer;
        if (list->GetString(i, &printer))
          printers->push_back(printer);
      }
    }
  } else {
    printing::PrinterList printer_list;
    scoped_refptr<printing::PrintBackend> backend(
        printing::PrintBackend::CreateInstance(NULL));
    if (backend)
      backend->EnumeratePrinters(&printer_list);
    for (size_t i = 0; i < printer_list.size(); ++i)
      printers->push_back(printer_list[i].printer_name);
  }
}

void CloudPrintProxyService::RefreshCloudPrintProxyStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  ServiceProcessControl::CloudPrintProxyInfoHandler callback =
       base::Bind(&CloudPrintProxyService::ProxyInfoCallback,
                  base::Unretained(this));
  // GetCloudPrintProxyInfo takes ownership of callback.
  process_control->GetCloudPrintProxyInfo(callback);
}

void CloudPrintProxyService::EnableCloudPrintProxy(const std::string& lsid,
                                                   const std::string& email) {
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  process_control->Send(new ServiceMsg_EnableCloudPrintProxy(lsid));
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, email);
}

void CloudPrintProxyService::EnableCloudPrintProxyWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email,
    bool connect_new_printers,
    const std::vector<std::string>& printer_blacklist) {
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  process_control->Send(
      new ServiceMsg_EnableCloudPrintProxyWithRobot(
          robot_auth_code, robot_email, user_email, connect_new_printers,
          printer_blacklist));
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, user_email);
}

void CloudPrintProxyService::DisableCloudPrintProxy() {
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  process_control->Send(new ServiceMsg_DisableCloudPrintProxy);
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
}

void CloudPrintProxyService::ProxyInfoCallback(
    const cloud_print::CloudPrintProxyInfo& proxy_info) {
  proxy_id_ = proxy_info.proxy_id;
  profile_->GetPrefs()->SetString(
      prefs::kCloudPrintEmail,
      proxy_info.enabled ? proxy_info.email : std::string());
  ApplyCloudPrintConnectorPolicy();
}

bool CloudPrintProxyService::InvokeServiceTask(const base::Closure& task) {
  GetServiceProcessControl()->Launch(task, base::Closure());
  return true;
}

ServiceProcessControl* CloudPrintProxyService::GetServiceProcessControl() {
  return ServiceProcessControl::GetInstance();
}
