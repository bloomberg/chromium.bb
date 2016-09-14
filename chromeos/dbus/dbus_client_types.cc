// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_client_types.h"

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace chromeos {

namespace {

// Command line switch mapping for --dbus-real-clients.
const struct {
  const char* param_name;
  DBusClientType client_type;
} kClientTypeMap[] = {
    {"bluetooth", DBusClientType::BLUETOOTH},
    {"cras", DBusClientType::CRAS},
    {"cros_disks", DBusClientType::CROS_DISKS},
    {"cryptohome", DBusClientType::CRYPTOHOME},
    {"debug_daemon", DBusClientType::DEBUG_DAEMON},
    {"easy_unlock", DBusClientType::EASY_UNLOCK},
    {"lorgnette_manager", DBusClientType::LORGNETTE_MANAGER},
    {"shill", DBusClientType::SHILL},
    {"gsm_sms", DBusClientType::GSM_SMS},
    {"image_burner", DBusClientType::IMAGE_BURNER},
    {"modem_messaging", DBusClientType::MODEM_MESSAGING},
    {"permission_broker", DBusClientType::PERMISSION_BROKER},
    {"power_manager", DBusClientType::POWER_MANAGER},
    {"session_manager", DBusClientType::SESSION_MANAGER},
    {"sms", DBusClientType::SMS},
    {"system_clock", DBusClientType::SYSTEM_CLOCK},
    {"update_engine", DBusClientType::UPDATE_ENGINE},
    {"arc_obb_mounter", DBusClientType::ARC_OBB_MOUNTER},
};

// Parses single command line param value for dbus subsystem. If successful,
// returns its enum representation. Otherwise returns NONE.
DBusClientType GetDBusClientType(const std::string& client_type_name) {
  for (size_t i = 0; i < arraysize(kClientTypeMap); i++) {
    if (base::LowerCaseEqualsASCII(client_type_name,
                                   kClientTypeMap[i].param_name))
      return kClientTypeMap[i].client_type;
  }
  return DBusClientType::NONE;
}

}  // namespace

DBusClientTypeMask ParseDBusRealClientsList(const std::string& clients_list) {
  DBusClientTypeMask real_client_mask = 0;
  for (const std::string& cur : base::SplitString(
           clients_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    DBusClientType client = GetDBusClientType(cur);
    if (client != DBusClientType::NONE) {
      LOG(WARNING) << "Forcing real dbus client for " << cur;
      real_client_mask |= static_cast<int>(client);
    } else {
      LOG(ERROR) << "Unknown dbus client: " << cur;
    }
  }

  return real_client_mask;
}

}  // namespace chromeos
