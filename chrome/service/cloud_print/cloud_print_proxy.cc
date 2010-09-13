// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"

CloudPrintProxy::CloudPrintProxy() {
}

CloudPrintProxy::~CloudPrintProxy() {
  DCHECK(CalledOnValidThread());
  Shutdown();
}

void CloudPrintProxy::Initialize(JsonPrefStore* service_prefs, Client* client) {
  DCHECK(CalledOnValidThread());
  service_prefs_ = service_prefs;
  client_ = client;
}

void CloudPrintProxy::EnableForUser(const std::string& lsid) {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    return;

  std::string proxy_id;
  service_prefs_->prefs()->GetString(prefs::kCloudPrintProxyId, &proxy_id);
  if (proxy_id.empty()) {
    proxy_id = cloud_print::PrintSystem::GenerateProxyId();
    service_prefs_->prefs()->SetString(prefs::kCloudPrintProxyId, proxy_id);
    service_prefs_->WritePrefs();
  }

  // Getting print system specific settings from the preferences.
  DictionaryValue* print_system_settings = NULL;
  service_prefs_->prefs()->GetDictionary(prefs::kCloudPrintPrintSystemSettings,
                                         &print_system_settings);

  // Check if there is an override for the cloud print server URL.
  std::string cloud_print_server_url_str;
  service_prefs_->prefs()->GetString(prefs::kCloudPrintServiceURL,
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
    service_prefs_->prefs()->GetString(prefs::kCloudPrintAuthToken,
                                       &cloud_print_token);
    DCHECK(!cloud_print_token.empty());
    std::string cloud_print_xmpp_token;
    service_prefs_->prefs()->GetString(prefs::kCloudPrintXMPPAuthToken,
                                       &cloud_print_xmpp_token);
    DCHECK(!cloud_print_xmpp_token.empty());
    std::string cloud_print_email;
    service_prefs_->prefs()->GetString(prefs::kCloudPrintEmail,
                                       &cloud_print_email);
    DCHECK(!cloud_print_email.empty());
    backend_->InitializeWithToken(cloud_print_token, cloud_print_xmpp_token,
                                  cloud_print_email, proxy_id);
  }
  if (client_) {
    client_->OnCloudPrintProxyEnabled();
  }
}

void CloudPrintProxy::DisableForUser() {
  DCHECK(CalledOnValidThread());
  Shutdown();
  if (client_) {
    client_->OnCloudPrintProxyDisabled();
  }
}

void CloudPrintProxy::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (backend_.get())
    backend_->Shutdown();
  backend_.reset();
}

// Notification methods from the backend. Called on UI thread.
void CloudPrintProxy::OnPrinterListAvailable(
    const cloud_print::PrinterList& printer_list) {
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
  service_prefs_->prefs()->SetString(prefs::kCloudPrintAuthToken,
                                     cloud_print_token);
  service_prefs_->prefs()->SetString(prefs::kCloudPrintXMPPAuthToken,
                                     cloud_print_xmpp_token);
  service_prefs_->prefs()->SetString(prefs::kCloudPrintEmail, email);
  service_prefs_->WritePrefs();
}

void CloudPrintProxy::OnAuthenticationFailed() {
  DCHECK(CalledOnValidThread());
  // If authenticated failed, we will disable the cloud print proxy.
  DisableForUser();
}

