// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/service/service_process.h"
#include "chrome/service/service_process_prefs.h"
#include "googleurl/src/gurl.h"

// This method is invoked on the IO thread to launch the browser process to
// display a desktop notification that the Cloud Print token is invalid and
// needs re-authentication.
static void ShowTokenExpiredNotificationInBrowser() {
  DCHECK(g_service_process->io_thread()->message_loop_proxy()->
      BelongsToCurrentThread());
  FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get browser process binary name.";
  }
  CommandLine cmd_line(exe_path);

  const CommandLine& process_command_line = *CommandLine::ForCurrentProcess();
  FilePath user_data_dir =
      process_command_line.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    cmd_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  cmd_line.AppendSwitch(switches::kNotifyCloudPrintTokenExpired);

  base::LaunchApp(cmd_line, false, false, NULL);
}

CloudPrintProxy::CloudPrintProxy()
    : service_prefs_(NULL),
      client_(NULL) {
}

CloudPrintProxy::~CloudPrintProxy() {
  DCHECK(CalledOnValidThread());
  Shutdown();
}

void CloudPrintProxy::Initialize(ServiceProcessPrefs* service_prefs,
                                 Client* client) {
  DCHECK(CalledOnValidThread());
  service_prefs_ = service_prefs;
  client_ = client;
}

void CloudPrintProxy::EnableForUser(const std::string& lsid) {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    return;

  std::string proxy_id;
  service_prefs_->GetString(prefs::kCloudPrintProxyId, &proxy_id);
  if (proxy_id.empty()) {
    proxy_id = cloud_print::PrintSystem::GenerateProxyId();
    service_prefs_->SetString(prefs::kCloudPrintProxyId, proxy_id);
    service_prefs_->WritePrefs();
  }

  // Getting print system specific settings from the preferences.
  DictionaryValue* print_system_settings = NULL;
  service_prefs_->GetDictionary(prefs::kCloudPrintPrintSystemSettings,
                                &print_system_settings);

  // Check if there is an override for the cloud print server URL.
  std::string cloud_print_server_url_str;
  service_prefs_->GetString(prefs::kCloudPrintServiceURL,
                            &cloud_print_server_url_str);
  if (cloud_print_server_url_str.empty()) {
    cloud_print_server_url_str = kDefaultCloudPrintServerUrl;
  }

  GURL cloud_print_server_url(cloud_print_server_url_str.c_str());
  DCHECK(cloud_print_server_url.is_valid());
  backend_.reset(new CloudPrintProxyBackend(this, cloud_print_server_url,
                                            print_system_settings));
  // If we have been passed in an LSID, we want to use this to authenticate.
  // Else we will try and retrieve the last used auth tokens from prefs.
  if (!lsid.empty()) {
    backend_->InitializeWithLsid(lsid, proxy_id);
  } else {
    std::string cloud_print_token;
    service_prefs_->GetString(prefs::kCloudPrintAuthToken,
                              &cloud_print_token);
    DCHECK(!cloud_print_token.empty());
    std::string cloud_print_xmpp_token;
    service_prefs_->GetString(prefs::kCloudPrintXMPPAuthToken,
                              &cloud_print_xmpp_token);
    DCHECK(!cloud_print_xmpp_token.empty());
    service_prefs_->GetString(prefs::kCloudPrintEmail,
                              &cloud_print_email_);
    DCHECK(!cloud_print_email_.empty());
    backend_->InitializeWithToken(cloud_print_token, cloud_print_xmpp_token,
                                  cloud_print_email_, proxy_id);
  }
  if (client_) {
    client_->OnCloudPrintProxyEnabled(true);
  }
}

void CloudPrintProxy::DisableForUser() {
  DCHECK(CalledOnValidThread());
  cloud_print_email_.clear();
  Shutdown();
  if (client_) {
    client_->OnCloudPrintProxyDisabled(true);
  }
}

bool CloudPrintProxy::IsEnabled(std::string* email) const {
  bool enabled = !cloud_print_email().empty();
  if (enabled && email) {
    *email = cloud_print_email();
  }
  return enabled;
}

// Notification methods from the backend. Called on UI thread.
void CloudPrintProxy::OnPrinterListAvailable(
    const printing::PrinterList& printer_list) {
  DCHECK(CalledOnValidThread());
  // We could potentially show UI here allowing the user to select which
  // printers to register. For now, we just register all.
  backend_->RegisterPrinters(printer_list);
}

void CloudPrintProxy::OnAuthenticated(
    const std::string& cloud_print_token,
    const std::string& cloud_print_xmpp_token,
    const std::string& email) {
  DCHECK(CalledOnValidThread());
  cloud_print_email_ = email;
  service_prefs_->SetString(prefs::kCloudPrintAuthToken,
                            cloud_print_token);
  service_prefs_->SetString(prefs::kCloudPrintXMPPAuthToken,
                            cloud_print_xmpp_token);
  service_prefs_->SetString(prefs::kCloudPrintEmail,
                            email);
  service_prefs_->WritePrefs();
}

void CloudPrintProxy::OnAuthenticationFailed() {
  DCHECK(CalledOnValidThread());
  // If authenticated failed, we will disable the cloud print proxy.
  DisableForUser();
  // Launch the browser to display a notification that the credentials have
  // expired.
  g_service_process->io_thread()->message_loop_proxy()->PostTask(
      FROM_HERE, NewRunnableFunction(&ShowTokenExpiredNotificationInBrowser));
}

void CloudPrintProxy::OnPrintSystemUnavailable() {
  // If the print system is unavailable, we want to shutdown the proxy and
  // disable it non-persistently.
  Shutdown();
  if (client_) {
    client_->OnCloudPrintProxyDisabled(false);
  }
}

void CloudPrintProxy::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    backend_->Shutdown();
  backend_.reset();
}
