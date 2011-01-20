// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/command_line_pref_store.h"

#include "app/app_switches.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/base/ui_base_switches.h"

const CommandLinePrefStore::StringSwitchToPreferenceMapEntry
    CommandLinePrefStore::string_switch_map_[] = {
      { switches::kLang, prefs::kApplicationLocale },
      { switches::kProxyServer, prefs::kProxyServer },
      { switches::kProxyPacUrl, prefs::kProxyPacUrl },
      { switches::kProxyBypassList, prefs::kProxyBypassList },
      { switches::kAuthSchemes, prefs::kAuthSchemes },
      { switches::kAuthServerWhitelist, prefs::kAuthServerWhitelist },
      { switches::kAuthNegotiateDelegateWhitelist,
          prefs::kAuthNegotiateDelegateWhitelist },
      { switches::kGSSAPILibraryName, prefs::kGSSAPILibraryName },
};

const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
    CommandLinePrefStore::boolean_switch_map_[] = {
      { switches::kDisableAuthNegotiateCnameLookup,
          prefs::kDisableAuthNegotiateCnameLookup, true },
      { switches::kEnableAuthNegotiatePort, prefs::kEnableAuthNegotiatePort,
          true },
      { switches::kDisable3DAPIs, prefs::kDisable3DAPIs, true },
};

CommandLinePrefStore::CommandLinePrefStore(const CommandLine* command_line)
    : command_line_(command_line) {
  ApplySimpleSwitches();
  ApplyProxyMode();
  ValidateProxySwitches();
}

CommandLinePrefStore::~CommandLinePrefStore() {}

void CommandLinePrefStore::ApplySimpleSwitches() {
  // Look for each switch we know about and set its preference accordingly.
  for (size_t i = 0; i < arraysize(string_switch_map_); ++i) {
    if (command_line_->HasSwitch(string_switch_map_[i].switch_name)) {
      Value* value = Value::CreateStringValue(command_line_->
          GetSwitchValueASCII(string_switch_map_[i].switch_name));
      SetValue(string_switch_map_[i].preference_path, value);
    }
  }

  for (size_t i = 0; i < arraysize(boolean_switch_map_); ++i) {
    if (command_line_->HasSwitch(boolean_switch_map_[i].switch_name)) {
      Value* value = Value::CreateBooleanValue(
          boolean_switch_map_[i].set_value);
      SetValue(boolean_switch_map_[i].preference_path, value);
    }
  }
}

bool CommandLinePrefStore::ValidateProxySwitches() {
  if (command_line_->HasSwitch(switches::kNoProxyServer) &&
      (command_line_->HasSwitch(switches::kProxyAutoDetect) ||
       command_line_->HasSwitch(switches::kProxyServer) ||
       command_line_->HasSwitch(switches::kProxyPacUrl) ||
       command_line_->HasSwitch(switches::kProxyBypassList))) {
    LOG(WARNING) << "Additional command-line proxy switches specified when --"
                 << switches::kNoProxyServer << " was also specified.";
    return false;
  }
  return true;
}

void CommandLinePrefStore::ApplyProxyMode() {
  if (command_line_->HasSwitch(switches::kNoProxyServer)) {
    SetValue(prefs::kProxyMode,
             Value::CreateIntegerValue(ProxyPrefs::MODE_DIRECT));
  } else if (command_line_->HasSwitch(switches::kProxyPacUrl)) {
    SetValue(prefs::kProxyMode,
             Value::CreateIntegerValue(ProxyPrefs::MODE_PAC_SCRIPT));
  } else if (command_line_->HasSwitch(switches::kProxyAutoDetect)) {
    SetValue(prefs::kProxyMode,
             Value::CreateIntegerValue(ProxyPrefs::MODE_AUTO_DETECT));
  } else if (command_line_->HasSwitch(switches::kProxyServer)) {
    SetValue(prefs::kProxyMode,
             Value::CreateIntegerValue(ProxyPrefs::MODE_FIXED_SERVERS));
  }
}
