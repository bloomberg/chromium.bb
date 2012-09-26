// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CONNECTOR_SETTINGS_H_
#define CHROME_SERVICE_CLOUD_PRINT_CONNECTOR_SETTINGS_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"

class ServiceProcessPrefs;

namespace base {
  class DictionaryValue;
}

class ConnectorSettings {
 public:
  ConnectorSettings();
  ~ConnectorSettings();

  void InitFrom(ServiceProcessPrefs* prefs);

  void CopyFrom(const ConnectorSettings& source);

  const GURL& server_url() const {
    return server_url_;
  };

  const std::string& proxy_id() const {
    return proxy_id_;
  }

  bool delete_on_enum_fail() const {
    return delete_on_enum_fail_;
  }

  bool connect_new_printers() const {
    return connect_new_printers_;
  };

  const base::DictionaryValue* print_system_settings() const {
    return print_system_settings_.get();
  };

  bool IsPrinterBlacklisted(const std::string& name) const;

 private:
  // Cloud Print server url.
  GURL server_url_;

  // This is initialized after a successful call to one of the Enable* methods.
  // It is not cleared in DisableUser.
  std::string proxy_id_;

  // If |true| printers that are not found locally will be deleted on GCP
  // even if the local enumeration failed.
  bool delete_on_enum_fail_;

  // If true register all new printers in cloud print.
  bool connect_new_printers_;

  // List of printers which should not be connected.
  std::set<std::string> printer_blacklist_;

  // Print system settings.
  scoped_ptr<base::DictionaryValue> print_system_settings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectorSettings);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CONNECTOR_SETTINGS_H_

