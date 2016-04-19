// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/command_line_pref_store.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/ssl_config/ssl_config_switches.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

const CommandLinePrefStore::StringSwitchToPreferenceMapEntry
    CommandLinePrefStore::string_switch_map_[] = {
      { switches::kLang, prefs::kApplicationLocale },
      { data_reduction_proxy::switches::kDataReductionProxy,
          data_reduction_proxy::prefs::kDataReductionProxy },
      { switches::kAuthServerWhitelist, prefs::kAuthServerWhitelist },
      { switches::kSSLVersionMin, ssl_config::prefs::kSSLVersionMin },
      { switches::kSSLVersionMax, ssl_config::prefs::kSSLVersionMax },
      { switches::kSSLVersionFallbackMin,
          ssl_config::prefs::kSSLVersionFallbackMin },
#if defined(OS_ANDROID)
      { switches::kAuthAndroidNegotiateAccountType,
          prefs::kAuthAndroidNegotiateAccountType },
#endif
};

const CommandLinePrefStore::PathSwitchToPreferenceMapEntry
    CommandLinePrefStore::path_switch_map_[] = {
      { switches::kDiskCacheDir, prefs::kDiskCacheDir },
};

const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
    CommandLinePrefStore::boolean_switch_map_[] = {
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
      { switches::kDisablePrintPreview, prefs::kPrintPreviewDisabled, true },
#if defined(OS_CHROMEOS)
      { chromeos::switches::kEnableTouchpadThreeFingerClick,
          prefs::kEnableTouchpadThreeFingerClick, true },
      { ash::switches::kAshEnableUnifiedDesktop,
          prefs::kUnifiedDesktopEnabledByDefault, true },
#endif
      { switches::kDisableAsyncDns, prefs::kBuiltInDnsClientEnabled, false },
};

const CommandLinePrefStore::IntegerSwitchToPreferenceMapEntry
    CommandLinePrefStore::integer_switch_map_[] = {
      { switches::kDiskCacheSize, prefs::kDiskCacheSize },
      { switches::kMediaCacheSize, prefs::kMediaCacheSize },
    };

CommandLinePrefStore::CommandLinePrefStore(
    const base::CommandLine* command_line)
    : command_line_(command_line) {
  ApplySimpleSwitches();
  ApplyProxyMode();
  ValidateProxySwitches();
  ApplySSLSwitches();
  ApplyBackgroundModeSwitches();
}

CommandLinePrefStore::~CommandLinePrefStore() {}

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

void CommandLinePrefStore::ApplySimpleSwitches() {
  // Look for each switch we know about and set its preference accordingly.
  for (size_t i = 0; i < arraysize(string_switch_map_); ++i) {
    if (command_line_->HasSwitch(string_switch_map_[i].switch_name)) {
      SetValue(string_switch_map_[i].preference_path,
               base::WrapUnique(
                   new base::StringValue(command_line_->GetSwitchValueASCII(
                       string_switch_map_[i].switch_name))),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }

  for (size_t i = 0; i < arraysize(path_switch_map_); ++i) {
    if (command_line_->HasSwitch(path_switch_map_[i].switch_name)) {
      SetValue(
          path_switch_map_[i].preference_path,
          base::WrapUnique(new base::StringValue(
              command_line_->GetSwitchValuePath(path_switch_map_[i].switch_name)
                  .value())),
          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
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
      SetValue(integer_switch_map_[i].preference_path,
               base::WrapUnique(new base::FundamentalValue(int_value)),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }

  for (size_t i = 0; i < arraysize(boolean_switch_map_); ++i) {
    if (command_line_->HasSwitch(boolean_switch_map_[i].switch_name)) {
      SetValue(boolean_switch_map_[i].preference_path,
               base::WrapUnique(new base::FundamentalValue(
                   boolean_switch_map_[i].set_value)),
               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    }
  }
}

void CommandLinePrefStore::ApplyProxyMode() {
  if (command_line_->HasSwitch(switches::kNoProxyServer)) {
    SetValue(proxy_config::prefs::kProxy,
             base::WrapUnique(ProxyConfigDictionary::CreateDirect()),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line_->HasSwitch(switches::kProxyPacUrl)) {
    std::string pac_script_url =
        command_line_->GetSwitchValueASCII(switches::kProxyPacUrl);
    SetValue(proxy_config::prefs::kProxy,
             base::WrapUnique(
                 ProxyConfigDictionary::CreatePacScript(pac_script_url, false)),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line_->HasSwitch(switches::kProxyAutoDetect)) {
    SetValue(proxy_config::prefs::kProxy,
             base::WrapUnique(ProxyConfigDictionary::CreateAutoDetect()),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line_->HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line_->GetSwitchValueASCII(switches::kProxyServer);
    std::string bypass_list =
        command_line_->GetSwitchValueASCII(switches::kProxyBypassList);
    SetValue(proxy_config::prefs::kProxy,
             base::WrapUnique(ProxyConfigDictionary::CreateFixedServers(
                 proxy_server, bypass_list)),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void CommandLinePrefStore::ApplySSLSwitches() {
  if (command_line_->HasSwitch(switches::kCipherSuiteBlacklist)) {
    std::unique_ptr<base::ListValue> list_value(new base::ListValue());
    list_value->AppendStrings(base::SplitString(
        command_line_->GetSwitchValueASCII(switches::kCipherSuiteBlacklist),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
    SetValue(ssl_config::prefs::kCipherSuiteBlacklist, std::move(list_value),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void CommandLinePrefStore::ApplyBackgroundModeSwitches() {
  if (command_line_->HasSwitch(switches::kDisableExtensions)) {
    SetValue(prefs::kBackgroundModeEnabled,
             base::WrapUnique(new base::FundamentalValue(false)),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}
