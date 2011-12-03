// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/net/ssl_config_service_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "net/base/ssl_cipher_suite_names.h"
#include "net/base/ssl_config_service.h"

using content::BrowserThread;

namespace {

// Converts a ListValue of StringValues into a vector of strings. Any Values
// which cannot be converted will be skipped.
std::vector<std::string> ListValueToStringVector(const ListValue* value) {
  std::vector<std::string> results;
  results.reserve(value->GetSize());
  std::string s;
  for (ListValue::const_iterator it = value->begin(); it != value->end();
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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServicePref

// An SSLConfigService which stores a cached version of the current SSLConfig
// prefs, which are updated by SSLConfigServiceManagerPref when the prefs
// change.
class SSLConfigServicePref : public net::SSLConfigService {
 public:
  SSLConfigServicePref() {}
  virtual ~SSLConfigServicePref() {}

  // Store SSL config settings in |config|. Must only be called from IO thread.
  virtual void GetSSLConfig(net::SSLConfig* config);

 private:
  // Allow the pref watcher to update our internal state.
  friend class SSLConfigServiceManagerPref;

  // This method is posted to the IO thread from the browser thread to carry the
  // new config information.
  void SetNewSSLConfig(const net::SSLConfig& new_config);

  // Cached value of prefs, should only be accessed from IO thread.
  net::SSLConfig cached_config_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServicePref);
};

void SSLConfigServicePref::GetSSLConfig(net::SSLConfig* config) {
  *config = cached_config_;
  config->crl_set = GetCRLSet();
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
    : public SSLConfigServiceManager,
      public content::NotificationObserver {
 public:
  explicit SSLConfigServiceManagerPref(PrefService* local_state);
  virtual ~SSLConfigServiceManagerPref() {}

  // Register local_state SSL preferences.
  static void RegisterPrefs(PrefService* prefs);

  virtual net::SSLConfigService* Get();

 private:
  // Callback for preference changes.  This will post the changes to the IO
  // thread with SetNewSSLConfig.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Store SSL config settings in |config|, directly from the preferences. Must
  // only be called from UI thread.
  void GetSSLConfigFromPrefs(net::SSLConfig* config);

  // Processes changes to the disabled cipher suites preference, updating the
  // cached list of parsed SSL/TLS cipher suites that are disabled.
  void OnDisabledCipherSuitesChange(PrefService* prefs);

  PrefChangeRegistrar pref_change_registrar_;

  // The prefs (should only be accessed from UI thread)
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember ssl3_enabled_;
  BooleanPrefMember tls1_enabled_;
  BooleanPrefMember origin_bound_certs_enabled_;

  // The cached list of disabled SSL cipher suites.
  std::vector<uint16> disabled_cipher_suites_;

  scoped_refptr<SSLConfigServicePref> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerPref);
};

SSLConfigServiceManagerPref::SSLConfigServiceManagerPref(
    PrefService* local_state)
    : ssl_config_service_(new SSLConfigServicePref()) {
  DCHECK(local_state);

  rev_checking_enabled_.Init(prefs::kCertRevocationCheckingEnabled,
                             local_state, this);
  ssl3_enabled_.Init(prefs::kSSL3Enabled, local_state, this);
  tls1_enabled_.Init(prefs::kTLS1Enabled, local_state, this);
  origin_bound_certs_enabled_.Init(prefs::kEnableOriginBoundCerts,
                                   local_state, this);
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(prefs::kCipherSuiteBlacklist, this);

  OnDisabledCipherSuitesChange(local_state);
  // Initialize from UI thread.  This is okay as there shouldn't be anything on
  // the IO thread trying to access it yet.
  GetSSLConfigFromPrefs(&ssl_config_service_->cached_config_);
}

// static
void SSLConfigServiceManagerPref::RegisterPrefs(PrefService* prefs) {
  net::SSLConfig default_config;
  prefs->RegisterBooleanPref(prefs::kCertRevocationCheckingEnabled,
                             default_config.rev_checking_enabled);
  prefs->RegisterBooleanPref(prefs::kSSL3Enabled,
                             default_config.ssl3_enabled);
  prefs->RegisterBooleanPref(prefs::kTLS1Enabled,
                             default_config.tls1_enabled);
  prefs->RegisterBooleanPref(prefs::kEnableOriginBoundCerts,
                             default_config.origin_bound_certs_enabled);
  prefs->RegisterListPref(prefs::kCipherSuiteBlacklist);
  // The Options menu used to allow changing the ssl.ssl3.enabled and
  // ssl.tls1.enabled preferences, so some users' Local State may have
  // these preferences.  Remove them from Local State.
  prefs->ClearPref(prefs::kSSL3Enabled);
  prefs->ClearPref(prefs::kTLS1Enabled);
}

net::SSLConfigService* SSLConfigServiceManagerPref::Get() {
  return ssl_config_service_;
}

void SSLConfigServiceManagerPref::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    std::string* pref_name_in = content::Details<std::string>(details).ptr();
    PrefService* prefs = content::Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kCipherSuiteBlacklist)
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
}

void SSLConfigServiceManagerPref::GetSSLConfigFromPrefs(
    net::SSLConfig* config) {
  config->rev_checking_enabled = rev_checking_enabled_.GetValue();
  config->ssl3_enabled = ssl3_enabled_.GetValue();
  config->tls1_enabled = tls1_enabled_.GetValue();
  config->disabled_cipher_suites = disabled_cipher_suites_;
  config->origin_bound_certs_enabled = origin_bound_certs_enabled_.GetValue();
  SSLConfigServicePref::SetSSLConfigFlags(config);
}

void SSLConfigServiceManagerPref::OnDisabledCipherSuitesChange(
    PrefService* prefs) {
  const ListValue* value = prefs->GetList(prefs::kCipherSuiteBlacklist);
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
void SSLConfigServiceManager::RegisterPrefs(PrefService* prefs) {
  SSLConfigServiceManagerPref::RegisterPrefs(prefs);
}
