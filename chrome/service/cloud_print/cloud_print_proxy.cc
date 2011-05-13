// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "chrome/common/net/gaia/gaia_oauth_client.h"
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
      client_(NULL),
      enabled_(false) {
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
  if (!CreateBackend())
    return;
  DCHECK(backend_.get());
  // Read persisted robot credentials because we may decide to reuse it if the
  // passed in LSID belongs the same user.
  std::string robot_refresh_token;
  service_prefs_->GetString(prefs::kCloudPrintRobotRefreshToken,
                            &robot_refresh_token);
  std::string robot_email;
  service_prefs_->GetString(prefs::kCloudPrintRobotEmail,
                            &robot_email);
  service_prefs_->GetString(prefs::kCloudPrintEmail, &user_email_);

  // If we have been passed in an LSID, we want to use this to authenticate.
  // Else we will try and retrieve the last used auth tokens from prefs.
  if (!lsid.empty()) {
    backend_->InitializeWithLsid(lsid,
                                 proxy_id_,
                                 robot_refresh_token,
                                 robot_email,
                                 user_email_);
  } else {
    // See if we have persisted robot credentials.
    if (!robot_refresh_token.empty()) {
      DCHECK(!robot_email.empty());
      backend_->InitializeWithRobotToken(robot_refresh_token,
                                         robot_email,
                                         proxy_id_);
    } else {
      // Finally see if we have persisted user credentials (legacy case).
      std::string cloud_print_token;
      service_prefs_->GetString(prefs::kCloudPrintAuthToken,
                                &cloud_print_token);
      DCHECK(!cloud_print_token.empty());
      backend_->InitializeWithToken(cloud_print_token, proxy_id_);
    }
  }
  if (client_) {
    client_->OnCloudPrintProxyEnabled(true);
  }
}

void CloudPrintProxy::EnableForUserWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email) {
  DCHECK(CalledOnValidThread());
  if (!CreateBackend())
    return;
  DCHECK(backend_.get());
  user_email_ = user_email;
  backend_->InitializeWithRobotAuthCode(robot_auth_code,
                                        robot_email,
                                        proxy_id_);
  if (client_) {
    client_->OnCloudPrintProxyEnabled(true);
  }
}

bool CloudPrintProxy::CreateBackend() {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    return false;

  service_prefs_->GetString(prefs::kCloudPrintProxyId, &proxy_id_);
  if (proxy_id_.empty()) {
    proxy_id_ = cloud_print::PrintSystem::GenerateProxyId();
    service_prefs_->SetString(prefs::kCloudPrintProxyId, proxy_id_);
    service_prefs_->WritePrefs();
  }

  // Getting print system specific settings from the preferences.
  const DictionaryValue* print_system_settings = NULL;
  service_prefs_->GetDictionary(prefs::kCloudPrintPrintSystemSettings,
                                &print_system_settings);

  // Check if there is an override for the cloud print server URL.
  std::string cloud_print_server_url_str;
  service_prefs_->GetString(prefs::kCloudPrintServiceURL,
                            &cloud_print_server_url_str);
  if (cloud_print_server_url_str.empty()) {
    cloud_print_server_url_str = kDefaultCloudPrintServerUrl;
  }

  // By default we don't poll for jobs when we lose XMPP connection. But this
  // behavior can be overridden by a preference.
  bool enable_job_poll = false;
  service_prefs_->GetBoolean(prefs::kCloudPrintEnableJobPoll,
                             &enable_job_poll);

  // TODO(sanjeevr): Allow overriding OAuthClientInfo in prefs.
  gaia::OAuthClientInfo oauth_client_info;
  oauth_client_info.client_id = kDefaultCloudPrintOAuthClientId;
  oauth_client_info.client_secret = kDefaultCloudPrintOAuthClientSecret;

  GURL cloud_print_server_url(cloud_print_server_url_str.c_str());
  DCHECK(cloud_print_server_url.is_valid());
  backend_.reset(new CloudPrintProxyBackend(this, cloud_print_server_url,
                                            print_system_settings,
                                            oauth_client_info,
                                            enable_job_poll));
  return true;
}

void CloudPrintProxy::DisableForUser() {
  DCHECK(CalledOnValidThread());
  user_email_.clear();
  enabled_ = false;
  Shutdown();
  if (client_) {
    client_->OnCloudPrintProxyDisabled(true);
  }
}

void CloudPrintProxy::GetProxyInfo(cloud_print::CloudPrintProxyInfo* info) {
  info->enabled = enabled_;
  info->email.clear();
  if (enabled_)
    info->email = user_email();
  info->proxy_id = proxy_id_;
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
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email) {
  DCHECK(CalledOnValidThread());
  service_prefs_->SetString(prefs::kCloudPrintRobotRefreshToken,
                            robot_oauth_refresh_token);
  service_prefs_->SetString(prefs::kCloudPrintRobotEmail,
                            robot_email);
  // If authenticating from a robot, the user email will be empty.
  if (!user_email.empty()) {
    user_email_ = user_email;
  }
  service_prefs_->SetString(prefs::kCloudPrintEmail, user_email_);
  enabled_ = true;
  DCHECK(!user_email_.empty());
  service_prefs_->WritePrefs();
}

void CloudPrintProxy::OnAuthenticationFailed() {
  DCHECK(CalledOnValidThread());
  // If authenticated failed, we will disable the cloud print proxy.
  DisableForUser();
  // Also delete the cached robot credentials since they may not be valid any
  // longer.
  service_prefs_->RemovePref(prefs::kCloudPrintRobotRefreshToken);
  service_prefs_->RemovePref(prefs::kCloudPrintRobotEmail);
  service_prefs_->WritePrefs();

  // Launch the browser to display a notification that the credentials have
  // expired (unless error dialogs are disabled).
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoErrorDialogs))
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

