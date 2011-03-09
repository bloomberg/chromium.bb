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
#include "chrome/browser/profiles/profile.h"
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
  explicit SSLConfigServiceManagerPref(Profile* profile);
  virtual ~SSLConfigServiceManagerPref() {}

  virtual net::SSLConfigService* Get();

 private:
  static void RegisterUserPrefs(PrefService* user_prefs);

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

SSLConfigServiceManagerPref::SSLConfigServiceManagerPref(Profile* profile)
    : ssl_config_service_(new SSLConfigServicePref()) {
  RegisterUserPrefs(profile->GetPrefs());

  rev_checking_enabled_.Init(prefs::kCertRevocationCheckingEnabled,
                             profile->GetPrefs(), this);
  ssl3_enabled_.Init(prefs::kSSL3Enabled, profile->GetPrefs(), this);
  tls1_enabled_.Init(prefs::kTLS1Enabled, profile->GetPrefs(), this);

  // Initialize from UI thread.  This is okay as there shouldn't be anything on
  // the IO thread trying to access it yet.
  GetSSLConfigFromPrefs(&ssl_config_service_->cached_config_);
}

// static
void SSLConfigServiceManagerPref::RegisterUserPrefs(PrefService* user_prefs) {
  net::SSLConfig default_config;
  user_prefs->RegisterBooleanPref(prefs::kCertRevocationCheckingEnabled,
                                  default_config.rev_checking_enabled);
  user_prefs->RegisterBooleanPref(prefs::kSSL3Enabled,
                                  default_config.ssl3_enabled);
  user_prefs->RegisterBooleanPref(prefs::kTLS1Enabled,
                                  default_config.tls1_enabled);
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
    Profile* profile) {
  return new SSLConfigServiceManagerPref(profile);
}
