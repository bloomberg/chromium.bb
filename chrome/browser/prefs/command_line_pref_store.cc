// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/command_line_pref_store.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/values.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/base/ui_base_switches.h"

const CommandLinePrefStore::StringSwitchToPreferenceMapEntry
    CommandLinePrefStore::string_switch_map_[] = {
      { switches::kLang, prefs::kApplicationLocale },
      { switches::kAuthSchemes, prefs::kAuthSchemes },
      { switches::kAuthServerWhitelist, prefs::kAuthServerWhitelist },
      { switches::kAuthNegotiateDelegateWhitelist,
          prefs::kAuthNegotiateDelegateWhitelist },
      { switches::kGSSAPILibraryName, prefs::kGSSAPILibraryName },
      { switches::kDiskCacheDir, prefs::kDiskCacheDir },
};

const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
    CommandLinePrefStore::boolean_switch_map_[] = {
      { switches::kDisableAuthNegotiateCnameLookup,
          prefs::kDisableAuthNegotiateCnameLookup, true },
      { switches::kEnableAuthNegotiatePort, prefs::kEnableAuthNegotiatePort,
          true },
      { switches::kDisable3DAPIs, prefs::kDisable3DAPIs, true },
      { switches::kEnableCloudPrintProxy, prefs::kCloudPrintProxyEnabled,
          true },
      { switches::kAllowOutdatedPlugins, prefs::kPluginsAllowOutdated, true },
      { switches::kAlwaysAuthorizePlugins, prefs::kPluginsAlwaysAuthorize,
          true },
      { switches::kNoPings, prefs::kEnableHyperlinkAuditing, false },
      { switches::kNoReferrers, prefs::kEnableReferrers, false },
      { switches::kAllowRunningInsecureContent,
        prefs::kWebKitAllowRunningInsecureContent, true },
      { switches::kNoDisplayingInsecureContent,
        prefs::kWebKitAllowDisplayingInsecureContent, false },
      { switches::kAllowCrossOriginAuthPrompt,
        prefs::kAllowCrossOriginAuthPrompt, true },
      { switches::kDisableSSL3, prefs::kSSL3Enabled, false },
      { switches::kDisableTLS1, prefs::kTLS1Enabled, false },
      { switches::kEnableOriginBoundCerts, prefs::kEnableOriginBoundCerts,
          true },
};

const CommandLinePrefStore::IntegerSwitchToPreferenceMapEntry
    CommandLinePrefStore::integer_switch_map_[] = {
      { switches::kDiskCacheSize, prefs::kDiskCacheSize },
      { switches::kMediaCacheSize, prefs::kMediaCacheSize },
    };

CommandLinePrefStore::CommandLinePrefStore(const CommandLine* command_line)
    : command_line_(command_line) {
  ApplySimpleSwitches();
  ApplyProxyMode();
  ValidateProxySwitches();
  ApplySSLSwitches();
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

  for (size_t i = 0; i < arraysize(integer_switch_map_); ++i) {
    if (command_line_->HasSwitch(integer_switch_map_[i].switch_name)) {
      std::string str_value = command_line_->GetSwitchValueASCII(
          integer_switch_map_[i].switch_name);
      int int_value = 0;
      if (!base::StringToInt(str_value, &int_value)) {
        LOG(ERROR) << "The value " << str_value << " of "
                   << integer_switch_map_[i].switch_name
                   << " can not be converted to integer, ignoring!";
        continue;
      }
      Value* value = Value::CreateIntegerValue(int_value);
      SetValue(integer_switch_map_[i].preference_path, value);
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
    SetValue(prefs::kProxy,
             ProxyConfigDictionary::CreateDirect());
  } else if (command_line_->HasSwitch(switches::kProxyPacUrl)) {
    std::string pac_script_url =
        command_line_->GetSwitchValueASCII(switches::kProxyPacUrl);
    SetValue(prefs::kProxy,
             ProxyConfigDictionary::CreatePacScript(pac_script_url, false));
  } else if (command_line_->HasSwitch(switches::kProxyAutoDetect)) {
    SetValue(prefs::kProxy,
             ProxyConfigDictionary::CreateAutoDetect());
  } else if (command_line_->HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line_->GetSwitchValueASCII(switches::kProxyServer);
    std::string bypass_list =
        command_line_->GetSwitchValueASCII(switches::kProxyBypassList);
    SetValue(prefs::kProxy,
             ProxyConfigDictionary::CreateFixedServers(proxy_server,
                                                       bypass_list));
  }
}

void CommandLinePrefStore::ApplySSLSwitches() {
  if (command_line_->HasSwitch(switches::kCipherSuiteBlacklist)) {
    std::string cipher_suites =
        command_line_->GetSwitchValueASCII(switches::kCipherSuiteBlacklist);
    std::vector<std::string> cipher_strings;
    base::SplitString(cipher_suites, ',', &cipher_strings);
    base::ListValue* list_value = new base::ListValue();
    for (std::vector<std::string>::const_iterator it = cipher_strings.begin();
         it != cipher_strings.end(); ++it) {
      list_value->Append(base::Value::CreateStringValue(*it));
    }
    SetValue(prefs::kCipherSuiteBlacklist, list_value);
  }
}
