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

ConnectorSettings::ConnectorSettings()
    : delete_on_enum_fail_(false),
      connect_new_printers_(true) {
}

ConnectorSettings::~ConnectorSettings() {
}

void ConnectorSettings::InitFrom(ServiceProcessPrefs* prefs) {
  CopyFrom(ConnectorSettings());

  proxy_id_ = prefs->GetString(prefs::kCloudPrintProxyId, "");
  if (proxy_id_.empty()) {
    proxy_id_ = cloud_print::PrintSystem::GenerateProxyId();
    prefs->SetString(prefs::kCloudPrintProxyId, proxy_id_);
    prefs->WritePrefs();
  }

  // Getting print system specific settings from the preferences.
  const base::DictionaryValue* print_system_settings =
      prefs->GetDictionary(prefs::kCloudPrintPrintSystemSettings);
  if (print_system_settings) {
    print_system_settings_.reset(print_system_settings->DeepCopy());
    // TODO(vitalybuka) : Consider to rename and move out option from
    // print_system_settings.
    print_system_settings_->GetBoolean(kDeleteOnEnumFail,
                                       &delete_on_enum_fail_);
  }

  // Check if there is an override for the cloud print server URL.
  server_url_ = GURL(prefs->GetString(prefs::kCloudPrintServiceURL, ""));
  DCHECK(server_url_.is_empty() || server_url_.is_valid());
  if (server_url_.is_empty() || !server_url_.is_valid()) {
    server_url_ = GURL(kDefaultCloudPrintServerUrl);
  }
  DCHECK(server_url_.is_valid());

  connect_new_printers_ = prefs->GetBoolean(
      prefs::kCloudPrintConnectNewPrinters, true);
  const base::ListValue* printers = prefs->GetList(
      prefs::kCloudPrintPrinterBlacklist);
  if (printers) {
    for (size_t i = 0; i < printers->GetSize(); ++i) {
      std::string printer;
      if (printers->GetString(i, &printer))
        printer_blacklist_.insert(printer);
    }
  }
}

bool ConnectorSettings::IsPrinterBlacklisted(const std::string& name) const {
  return printer_blacklist_.find(name) != printer_blacklist_.end();
};

void ConnectorSettings::CopyFrom(const ConnectorSettings& source) {
  server_url_ = source.server_url();
  proxy_id_ = source.proxy_id();
  delete_on_enum_fail_ = source.delete_on_enum_fail();
  connect_new_printers_ = source.connect_new_printers();
  printer_blacklist_ = source.printer_blacklist_;
  if (source.print_system_settings())
    print_system_settings_.reset(source.print_system_settings()->DeepCopy());
}

