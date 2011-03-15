// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "net/base/ssl_config_service.h"

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
      public NotificationObserver {
 public:
  SSLConfigServiceManagerPref(PrefService* user_prefs,
                              PrefService* local_state);
  virtual ~SSLConfigServiceManagerPref() {}

  virtual net::SSLConfigService* Get();

 private:
  // Register user_prefs and local_state SSL preferences.
  static void RegisterPrefs(PrefService* prefs);

  // Copy pref values to local_state from user_prefs if local_state doesn't have
  // the pref value and user_prefs has the pref value. Remove them from
  // user_prefs.
  static void MigrateUserPrefs(PrefService* local_state,
                               PrefService* user_prefs);

  // Callback for preference changes.  This will post the changes to the IO
  // thread with SetNewSSLConfig.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Store SSL config settings in |config|, directly from the preferences. Must
  // only be called from UI thread.
  void GetSSLConfigFromPrefs(net::SSLConfig* config);

  // The prefs (should only be accessed from UI thread)
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember ssl3_enabled_;
  BooleanPrefMember tls1_enabled_;

  scoped_refptr<SSLConfigServicePref> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerPref);
};

SSLConfigServiceManagerPref::SSLConfigServiceManagerPref(
    PrefService* user_prefs, PrefService* local_state)
    : ssl_config_service_(new SSLConfigServicePref()) {
  DCHECK(user_prefs);
  DCHECK(local_state);

  RegisterPrefs(user_prefs);
  RegisterPrefs(local_state);

  // TODO(rtenneti): remove migration code after 6 months.
  MigrateUserPrefs(local_state, user_prefs);

  rev_checking_enabled_.Init(prefs::kCertRevocationCheckingEnabled,
                             local_state, this);
  ssl3_enabled_.Init(prefs::kSSL3Enabled, local_state, this);
  tls1_enabled_.Init(prefs::kTLS1Enabled, local_state, this);

  // Initialize from UI thread.  This is okay as there shouldn't be anything on
  // the IO thread trying to access it yet.
  GetSSLConfigFromPrefs(&ssl_config_service_->cached_config_);
}

// static
void SSLConfigServiceManagerPref::RegisterPrefs(PrefService* prefs) {
  net::SSLConfig default_config;
  if (!prefs->FindPreference(prefs::kCertRevocationCheckingEnabled)) {
    prefs->RegisterBooleanPref(prefs::kCertRevocationCheckingEnabled,
                               default_config.rev_checking_enabled);
  }
  if (!prefs->FindPreference(prefs::kSSL3Enabled)) {
    prefs->RegisterBooleanPref(prefs::kSSL3Enabled,
                               default_config.ssl3_enabled);
  }
  if (!prefs->FindPreference(prefs::kTLS1Enabled)) {
    prefs->RegisterBooleanPref(prefs::kTLS1Enabled,
                               default_config.tls1_enabled);
  }
}

// static
void SSLConfigServiceManagerPref::MigrateUserPrefs(PrefService* local_state,
                                                   PrefService* user_prefs) {
  if (user_prefs->HasPrefPath(prefs::kCertRevocationCheckingEnabled)) {
    if (!local_state->HasPrefPath(prefs::kCertRevocationCheckingEnabled)) {
      // Migrate the kCertRevocationCheckingEnabled preference.
      local_state->SetBoolean(prefs::kCertRevocationCheckingEnabled,
          user_prefs->GetBoolean(prefs::kCertRevocationCheckingEnabled));
    }
    user_prefs->ClearPref(prefs::kCertRevocationCheckingEnabled);
  }
  if (user_prefs->HasPrefPath(prefs::kSSL3Enabled)) {
    if (!local_state->HasPrefPath(prefs::kSSL3Enabled)) {
      // Migrate the kSSL3Enabled preference.
      local_state->SetBoolean(prefs::kSSL3Enabled,
          user_prefs->GetBoolean(prefs::kSSL3Enabled));
    }
    user_prefs->ClearPref(prefs::kSSL3Enabled);
  }
  if (user_prefs->HasPrefPath(prefs::kTLS1Enabled)) {
    if (!local_state->HasPrefPath(prefs::kTLS1Enabled)) {
      // Migrate the kTLS1Enabled preference.
      local_state->SetBoolean(prefs::kTLS1Enabled,
          user_prefs->GetBoolean(prefs::kTLS1Enabled));
    }
    user_prefs->ClearPref(prefs::kTLS1Enabled);
  }
}

net::SSLConfigService* SSLConfigServiceManagerPref::Get() {
  return ssl_config_service_;
}

void SSLConfigServiceManagerPref::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  base::Thread* io_thread = g_browser_process->io_thread();
  if (io_thread) {
    net::SSLConfig new_config;
    GetSSLConfigFromPrefs(&new_config);

    // Post a task to |io_loop| with the new configuration, so it can
    // update |cached_config_|.
    io_thread->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            ssl_config_service_.get(),
            &SSLConfigServicePref::SetNewSSLConfig,
            new_config));
  }
}

void SSLConfigServiceManagerPref::GetSSLConfigFromPrefs(
    net::SSLConfig* config) {
  config->rev_checking_enabled = rev_checking_enabled_.GetValue();
  config->ssl3_enabled = ssl3_enabled_.GetValue();
  config->tls1_enabled = tls1_enabled_.GetValue();
  SSLConfigServicePref::SetSSLConfigFlags(config);
}

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    PrefService* user_prefs,
    PrefService* local_state) {
  return new SSLConfigServiceManagerPref(user_prefs, local_state);
}
