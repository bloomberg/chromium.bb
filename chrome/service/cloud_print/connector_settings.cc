// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/connector_settings.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/service/service_process_prefs.h"

namespace {

const char kDefaultCloudPrintServerUrl[] = "https://www.google.com/cloudprint";
const char kDeleteOnEnumFail[] = "delete_on_enum_fail";

}  // namespace

ConnectorSettings::ConnectorSettings() : delete_on_enum_fail_(false) {
}

ConnectorSettings::~ConnectorSettings() {
}

void ConnectorSettings::InitFrom(ServiceProcessPrefs* prefs) {
  CopyFrom(ConnectorSettings());

  prefs->GetString(prefs::kCloudPrintProxyId, &proxy_id_);
  if (proxy_id_.empty()) {
    proxy_id_ = cloud_print::PrintSystem::GenerateProxyId();
    prefs->SetString(prefs::kCloudPrintProxyId, proxy_id_);
    prefs->WritePrefs();
  }

  // Getting print system specific settings from the preferences.
  const base::DictionaryValue* print_system_settings = NULL;
  prefs->GetDictionary(prefs::kCloudPrintPrintSystemSettings,
                       &print_system_settings);
  if (print_system_settings) {
    print_system_settings_.reset(print_system_settings->DeepCopy());
    // TODO(vitalybuka) : Consider to move option from print_system_settings.
    print_system_settings_->GetBoolean(kDeleteOnEnumFail,
                                       &delete_on_enum_fail_);
  }

  // Check if there is an override for the cloud print server URL.
  std::string cloud_print_server_url_str;
  prefs->GetString(prefs::kCloudPrintServiceURL,
                   &cloud_print_server_url_str);
  if (cloud_print_server_url_str.empty()) {
    cloud_print_server_url_str = kDefaultCloudPrintServerUrl;
  }
  server_url_ = GURL(cloud_print_server_url_str.c_str());
  DCHECK(server_url_.is_valid());
}

void ConnectorSettings::CopyFrom(const ConnectorSettings& source) {
  server_url_ = source.server_url();
  proxy_id_ = source.proxy_id();
  delete_on_enum_fail_ = source.delete_on_enum_fail();
  if (source.print_system_settings())
    print_system_settings_.reset(source.print_system_settings()->DeepCopy());
}

