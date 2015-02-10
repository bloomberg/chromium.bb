// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/net/ssl_config_service_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_config_service.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Field trial for ClientHello padding.
const char kClientHelloFieldTrialName[] = "FastRadioPadding";
const char kClientHelloFieldTrialEnabledGroupName[] = "Enabled";

// Converts a ListValue of StringValues into a vector of strings. Any Values
// which cannot be converted will be skipped.
std::vector<std::string> ListValueToStringVector(const base::ListValue* value) {
  std::vector<std::string> results;
  results.reserve(value->GetSize());
  std::string s;
  for (base::ListValue::const_iterator it = value->begin(); it != value->end();
       ++it) {
    if (!(*it)->GetAsString(&s))
      continue;
    results.push_back(s);
  }
  return results;
}

// Parses a vector of cipher suite strings, returning a sorted vector
// containing the underlying SSL/TLS cipher suites. Unrecognized/invalid
// cipher suites will be ignored.
std::vector<uint16> ParseCipherSuites(
    const std::vector<std::string>& cipher_strings) {
  std::vector<uint16> cipher_suites;
  cipher_suites.reserve(cipher_strings.size());

  for (std::vector<std::string>::const_iterator it = cipher_strings.begin();
       it != cipher_strings.end(); ++it) {
    uint16 cipher_suite = 0;
    if (!net::ParseSSLCipherString(*it, &cipher_suite)) {
      LOG(ERROR) << "Ignoring unrecognized or unparsable cipher suite: "
                 << *it;
      continue;
    }
    cipher_suites.push_back(cipher_suite);
  }
  std::sort(cipher_suites.begin(), cipher_suites.end());
  return cipher_suites;
}

// Returns the SSL protocol version (as a uint16) represented by a string.
// Returns 0 if the string is invalid.
uint16 SSLProtocolVersionFromString(const std::string& version_str) {
  uint16 version = 0;  // Invalid.
  if (version_str == switches::kSSLVersionSSLv3) {
    version = net::SSL_PROTOCOL_VERSION_SSL3;
  } else if (version_str == switches::kSSLVersionTLSv1) {
    version = net::SSL_PROTOCOL_VERSION_TLS1;
  } else if (version_str == switches::kSSLVersionTLSv11) {
    version = net::SSL_PROTOCOL_VERSION_TLS1_1;
  } else if (version_str == switches::kSSLVersionTLSv12) {
    version = net::SSL_PROTOCOL_VERSION_TLS1_2;
  }
  return version;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServicePref

// An SSLConfigService which stores a cached version of the current SSLConfig
// prefs, which are updated by SSLConfigServiceManagerPref when the prefs
// change.
class SSLConfigServicePref : public net::SSLConfigService {
 public:
  SSLConfigServicePref() {}

  // Store SSL config settings in |config|. Must only be called from IO thread.
  void GetSSLConfig(net::SSLConfig* config) override;

  bool SupportsFastradioPadding(const GURL& url) override;

 private:
  // Allow the pref watcher to update our internal state.
  friend class SSLConfigServiceManagerPref;

  ~SSLConfigServicePref() override {}

  // This method is posted to the IO thread from the browser thread to carry the
  // new config information.
  void SetNewSSLConfig(const net::SSLConfig& new_config);

  // Cached value of prefs, should only be accessed from IO thread.
  net::SSLConfig cached_config_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServicePref);
};

void SSLConfigServicePref::GetSSLConfig(net::SSLConfig* config) {
  *config = cached_config_;
}

bool SSLConfigServicePref::SupportsFastradioPadding(const GURL& url) {
  return google_util::IsGoogleHostname(url.host(),
                                       google_util::ALLOW_SUBDOMAIN);
}

void SSLConfigServicePref::SetNewSSLConfig(
    const net::SSLConfig& new_config) {
  net::SSLConfig orig_config = cached_config_;
  cached_config_ = new_config;
  ProcessConfigUpdate(orig_config, new_config);
}

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManagerPref

// The manager for holding and updating an SSLConfigServicePref instance.
class SSLConfigServiceManagerPref
    : public SSLConfigServiceManager {
 public:
  explicit SSLConfigServiceManagerPref(PrefService* local_state);
  ~SSLConfigServiceManagerPref() override {}

  // Register local_state SSL preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  net::SSLConfigService* Get() override;

 private:
  // Callback for preference changes.  This will post the changes to the IO
  // thread with SetNewSSLConfig.
  void OnPreferenceChanged(PrefService* prefs,
                           const std::string& pref_name);

  // Store SSL config settings in |config|, directly from the preferences. Must
  // only be called from UI thread.
  void GetSSLConfigFromPrefs(net::SSLConfig* config);

  // Processes changes to the disabled cipher suites preference, updating the
  // cached list of parsed SSL/TLS cipher suites that are disabled.
  void OnDisabledCipherSuitesChange(PrefService* local_state);

  PrefChangeRegistrar local_state_change_registrar_;

  // The local_state prefs (should only be accessed from UI thread)
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember rev_checking_required_local_anchors_;
  StringPrefMember ssl_version_min_;
  StringPrefMember ssl_version_max_;
  StringPrefMember ssl_version_fallback_min_;
  BooleanPrefMember ssl_record_splitting_disabled_;

  // The cached list of disabled SSL cipher suites.
  std::vector<uint16> disabled_cipher_suites_;

  scoped_refptr<SSLConfigServicePref> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerPref);
};

SSLConfigServiceManagerPref::SSLConfigServiceManagerPref(
    PrefService* local_state)
    : ssl_config_service_(new SSLConfigServicePref()) {
  DCHECK(local_state);

  PrefChangeRegistrar::NamedChangeCallback local_state_callback = base::Bind(
      &SSLConfigServiceManagerPref::OnPreferenceChanged,
      base::Unretained(this),
      local_state);

  rev_checking_enabled_.Init(
      prefs::kCertRevocationCheckingEnabled, local_state, local_state_callback);
  rev_checking_required_local_anchors_.Init(
      prefs::kCertRevocationCheckingRequiredLocalAnchors,
      local_state,
      local_state_callback);
  ssl_version_min_.Init(
      prefs::kSSLVersionMin, local_state, local_state_callback);
  ssl_version_max_.Init(
      prefs::kSSLVersionMax, local_state, local_state_callback);
  ssl_version_fallback_min_.Init(
      prefs::kSSLVersionFallbackMin, local_state, local_state_callback);
  ssl_record_splitting_disabled_.Init(
      prefs::kDisableSSLRecordSplitting, local_state, local_state_callback);

  local_state_change_registrar_.Init(local_state);
  local_state_change_registrar_.Add(
      prefs::kCipherSuiteBlacklist, local_state_callback);

  OnDisabledCipherSuitesChange(local_state);

  // Initialize from UI thread.  This is okay as there shouldn't be anything on
  // the IO thread trying to access it yet.
  GetSSLConfigFromPrefs(&ssl_config_service_->cached_config_);
}

// static
void SSLConfigServiceManagerPref::RegisterPrefs(PrefRegistrySimple* registry) {
  net::SSLConfig default_config;
  registry->RegisterBooleanPref(prefs::kCertRevocationCheckingEnabled,
                                default_config.rev_checking_enabled);
  registry->RegisterBooleanPref(
      prefs::kCertRevocationCheckingRequiredLocalAnchors,
      default_config.rev_checking_required_local_anchors);
  registry->RegisterStringPref(prefs::kSSLVersionMin, "");
  registry->RegisterStringPref(prefs::kSSLVersionMax, "");
  registry->RegisterStringPref(prefs::kSSLVersionFallbackMin, "");
  registry->RegisterBooleanPref(prefs::kDisableSSLRecordSplitting,
                                !default_config.false_start_enabled);
  registry->RegisterListPref(prefs::kCipherSuiteBlacklist);
}

net::SSLConfigService* SSLConfigServiceManagerPref::Get() {
  return ssl_config_service_.get();
}

void SSLConfigServiceManagerPref::OnPreferenceChanged(
    PrefService* prefs,
    const std::string& pref_name_in) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs);
  if (pref_name_in == prefs::kCipherSuiteBlacklist)
    OnDisabledCipherSuitesChange(prefs);

  net::SSLConfig new_config;
  GetSSLConfigFromPrefs(&new_config);

  // Post a task to |io_loop| with the new configuration, so it can
  // update |cached_config_|.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &SSLConfigServicePref::SetNewSSLConfig,
          ssl_config_service_.get(),
          new_config));
}

void SSLConfigServiceManagerPref::GetSSLConfigFromPrefs(
    net::SSLConfig* config) {
  // rev_checking_enabled was formerly a user-settable preference, but now
  // it is managed-only.
  if (rev_checking_enabled_.IsManaged())
    config->rev_checking_enabled = rev_checking_enabled_.GetValue();
  else
    config->rev_checking_enabled = false;
  config->rev_checking_required_local_anchors =
      rev_checking_required_local_anchors_.GetValue();
  std::string version_min_str = ssl_version_min_.GetValue();
  std::string version_max_str = ssl_version_max_.GetValue();
  std::string version_fallback_min_str = ssl_version_fallback_min_.GetValue();
  config->version_min = net::kDefaultSSLVersionMin;
  config->version_max = net::SSLClientSocket::GetMaxSupportedSSLVersion();
  config->version_fallback_min = net::kDefaultSSLVersionFallbackMin;
  uint16 version_min = SSLProtocolVersionFromString(version_min_str);
  uint16 version_max = SSLProtocolVersionFromString(version_max_str);
  uint16 version_fallback_min =
      SSLProtocolVersionFromString(version_fallback_min_str);
  if (version_min) {
    config->version_min = version_min;
  } else {
    const std::string group = base::FieldTrialList::FindFullName("SSLv3");
    if (group == "Enabled") {
      config->version_min = net::SSL_PROTOCOL_VERSION_SSL3;
    }
  }
  if (version_max) {
    uint16 supported_version_max = config->version_max;
    config->version_max = std::min(supported_version_max, version_max);
  }
  if (version_fallback_min) {
    config->version_fallback_min = version_fallback_min;
  }
  config->disabled_cipher_suites = disabled_cipher_suites_;
  // disabling False Start also happens to disable record splitting.
  config->false_start_enabled = !ssl_record_splitting_disabled_.GetValue();

  base::StringPiece group =
      base::FieldTrialList::FindFullName(kClientHelloFieldTrialName);
  if (group.starts_with(kClientHelloFieldTrialEnabledGroupName)) {
    config->fastradio_padding_enabled = true;
  }
}

void SSLConfigServiceManagerPref::OnDisabledCipherSuitesChange(
    PrefService* local_state) {
  const base::ListValue* value =
      local_state->GetList(prefs::kCipherSuiteBlacklist);
  disabled_cipher_suites_ = ParseCipherSuites(ListValueToStringVector(value));
}

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    PrefService* local_state) {
  return new SSLConfigServiceManagerPref(local_state);
}

// static
void SSLConfigServiceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  SSLConfigServiceManagerPref::RegisterPrefs(registry);
}
