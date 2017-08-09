// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_command_line_pref_store.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/ssl_config/ssl_config_switches.h"
#include "components/sync/base/pref_names.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

const CommandLinePrefStore::SwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::string_switch_map_[] = {
        {switches::kLang, prefs::kApplicationLocale},
        {data_reduction_proxy::switches::kDataReductionProxy,
         data_reduction_proxy::prefs::kDataReductionProxy},
        {switches::kAuthServerWhitelist, prefs::kAuthServerWhitelist},
        {switches::kSSLVersionMin, ssl_config::prefs::kSSLVersionMin},
        {switches::kTLS13Variant, ssl_config::prefs::kTLS13Variant},
#if defined(OS_ANDROID)
        {switches::kAuthAndroidNegotiateAccountType,
         prefs::kAuthAndroidNegotiateAccountType},
#endif
};

const CommandLinePrefStore::SwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::path_switch_map_[] = {
      { switches::kDiskCacheDir, prefs::kDiskCacheDir },
      { switches::kLocalSyncBackendDir, syncer::prefs::kLocalSyncBackendDir },
};

const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::boolean_switch_map_[] = {
        {switches::kDisable3DAPIs, prefs::kDisable3DAPIs, true},
        {switches::kEnableCloudPrintProxy, prefs::kCloudPrintProxyEnabled,
         true},
        {switches::kAllowOutdatedPlugins, prefs::kPluginsAllowOutdated, true},
        {switches::kAlwaysAuthorizePlugins, prefs::kPluginsAlwaysAuthorize,
         true},
        {switches::kNoPings, prefs::kEnableHyperlinkAuditing, false},
        {switches::kNoReferrers, prefs::kEnableReferrers, false},
        {switches::kAllowRunningInsecureContent,
         prefs::kWebKitAllowRunningInsecureContent, true},
        {switches::kAllowCrossOriginAuthPrompt,
         prefs::kAllowCrossOriginAuthPrompt, true},
        {switches::kDisablePrintPreview, prefs::kPrintPreviewDisabled, true},
#if defined(OS_CHROMEOS)
        {chromeos::switches::kEnableTouchpadThreeFingerClick,
         prefs::kEnableTouchpadThreeFingerClick, true},
        {switches::kEnableUnifiedDesktop,
         prefs::kUnifiedDesktopEnabledByDefault, true},
        {chromeos::switches::kEnableCastReceiver, prefs::kCastReceiverEnabled,
         true},
#endif
        {switches::kUnsafePacUrl, prefs::kPacHttpsUrlStrippingEnabled, false},
        {switches::kEnableLocalSyncBackend,
         syncer::prefs::kEnableLocalSyncBackend, true},
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
        {switches::kUseSystemDefaultPrinter,
         prefs::kPrintPreviewUseSystemDefaultPrinter, true},
#endif
};

const CommandLinePrefStore::SwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::integer_switch_map_[] = {
      { switches::kDiskCacheSize, prefs::kDiskCacheSize },
      { switches::kMediaCacheSize, prefs::kMediaCacheSize },
    };

ChromeCommandLinePrefStore::ChromeCommandLinePrefStore(
    const base::CommandLine* command_line)
    : CommandLinePrefStore(command_line) {
  ApplySimpleSwitches();
  ApplyProxyMode();
  ValidateProxySwitches();
  ApplySSLSwitches();
  ApplyBackgroundModeSwitches();
}

ChromeCommandLinePrefStore::~ChromeCommandLinePrefStore() {}

bool ChromeCommandLinePrefStore::ValidateProxySwitches() {
  if (command_line()->HasSwitch(switches::kNoProxyServer) &&
      (command_line()->HasSwitch(switches::kProxyAutoDetect) ||
       command_line()->HasSwitch(switches::kProxyServer) ||
       command_line()->HasSwitch(switches::kProxyPacUrl) ||
       command_line()->HasSwitch(switches::kProxyBypassList))) {
    LOG(WARNING) << "Additional command-line proxy switches specified when --"
                 << switches::kNoProxyServer << " was also specified.";
    return false;
  }
  return true;
}

void ChromeCommandLinePrefStore::ApplySimpleSwitches() {
  // Look for each switch we know about and set its preference accordingly.
  ApplyStringSwitches(string_switch_map_, arraysize(string_switch_map_));
  ApplyPathSwitches(path_switch_map_, arraysize(path_switch_map_));
  ApplyIntegerSwitches(integer_switch_map_, arraysize(integer_switch_map_));
  ApplyBooleanSwitches(boolean_switch_map_, arraysize(boolean_switch_map_));
}

void ChromeCommandLinePrefStore::ApplyProxyMode() {
  if (command_line()->HasSwitch(switches::kNoProxyServer)) {
    SetValue(proxy_config::prefs::kProxy, ProxyConfigDictionary::CreateDirect(),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line()->HasSwitch(switches::kProxyPacUrl)) {
    std::string pac_script_url =
        command_line()->GetSwitchValueASCII(switches::kProxyPacUrl);
    SetValue(proxy_config::prefs::kProxy,
             ProxyConfigDictionary::CreatePacScript(pac_script_url, false),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line()->HasSwitch(switches::kProxyAutoDetect)) {
    SetValue(proxy_config::prefs::kProxy,
             ProxyConfigDictionary::CreateAutoDetect(),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line()->HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line()->GetSwitchValueASCII(switches::kProxyServer);
    std::string bypass_list =
        command_line()->GetSwitchValueASCII(switches::kProxyBypassList);
    SetValue(
        proxy_config::prefs::kProxy,
        ProxyConfigDictionary::CreateFixedServers(proxy_server, bypass_list),
        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void ChromeCommandLinePrefStore::ApplySSLSwitches() {
  if (command_line()->HasSwitch(switches::kCipherSuiteBlacklist)) {
    std::unique_ptr<base::ListValue> list_value(new base::ListValue());
    list_value->AppendStrings(base::SplitString(
        command_line()->GetSwitchValueASCII(switches::kCipherSuiteBlacklist),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
    SetValue(ssl_config::prefs::kCipherSuiteBlacklist, std::move(list_value),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }

  // If a non-disabled TLS 1.3 variant flag is set, enable TLS 1.3 in
  // SSLVersionMax.
  if (command_line()->HasSwitch(switches::kTLS13Variant) &&
      command_line()->GetSwitchValueASCII(switches::kTLS13Variant) !=
          switches::kTLS13VariantDisabled) {
    SetValue(ssl_config::prefs::kSSLVersionMax,
             base::MakeUnique<base::Value>(switches::kSSLVersionTLSv13),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void ChromeCommandLinePrefStore::ApplyBackgroundModeSwitches() {
  if (command_line()->HasSwitch(switches::kDisableExtensions)) {
    SetValue(prefs::kBackgroundModeEnabled,
             base::MakeUnique<base::Value>(false),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}
