// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/pref_proxy_config_service.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

PrefProxyConfigTracker::PrefProxyConfigTracker(PrefService* pref_service)
    : pref_service_(pref_service) {
  valid_ = ReadPrefConfig(&pref_config_);
  proxy_prefs_observer_.reset(
      PrefSetObserver::CreateProxyPrefSetObserver(pref_service_, this));
}

PrefProxyConfigTracker::~PrefProxyConfigTracker() {
  DCHECK(pref_service_ == NULL);
}

bool PrefProxyConfigTracker::GetProxyConfig(net::ProxyConfig* config) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (valid_)
    *config = pref_config_;
  return valid_;
}

void PrefProxyConfigTracker::DetachFromPrefService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Stop notifications.
  proxy_prefs_observer_.reset();
  pref_service_ = NULL;
}

void PrefProxyConfigTracker::AddObserver(
    PrefProxyConfigTracker::ObserverInterface* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.AddObserver(observer);
}

void PrefProxyConfigTracker::RemoveObserver(
    PrefProxyConfigTracker::ObserverInterface* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.RemoveObserver(observer);
}

void PrefProxyConfigTracker::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == NotificationType::PREF_CHANGED &&
      Source<PrefService>(source).ptr() == pref_service_) {
    net::ProxyConfig new_config;
    bool valid = ReadPrefConfig(&new_config);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &PrefProxyConfigTracker::InstallProxyConfig,
                          new_config, valid));
  } else {
    NOTREACHED() << "Unexpected notification of type " << type.value;
  }
}

void PrefProxyConfigTracker::InstallProxyConfig(const net::ProxyConfig& config,
                                                bool valid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (valid_ != valid || !pref_config_.Equals(config)) {
    valid_ = valid;
    if (valid_)
      pref_config_ = config;
    FOR_EACH_OBSERVER(ObserverInterface, observers_,
                      OnPrefProxyConfigChanged());
  }
}

bool PrefProxyConfigTracker::ReadPrefConfig(net::ProxyConfig* config) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Clear the configuration.
  *config = net::ProxyConfig();

  // Scan for all "enable" type proxy switches.
  static const char* proxy_prefs[] = {
    prefs::kProxyPacUrl,
    prefs::kProxyServer,
    prefs::kProxyBypassList,
    prefs::kProxyAutoDetect
  };

  // Check whether the preference system holds a valid proxy configuration. Note
  // that preferences coming from a lower-priority source than the user settings
  // are ignored. That's because chrome treats the system settings as the
  // default values, which should apply if there's no explicit value forced by
  // policy or the user.
  bool found_enable_proxy_pref = false;
  for (size_t i = 0; i < arraysize(proxy_prefs); i++) {
    const PrefService::Preference* pref =
        pref_service_->FindPreference(proxy_prefs[i]);
    DCHECK(pref);
    if (pref && (!pref->IsUserModifiable() || pref->HasUserSetting())) {
      found_enable_proxy_pref = true;
      break;
    }
  }

  if (!found_enable_proxy_pref &&
      !pref_service_->GetBoolean(prefs::kNoProxyServer)) {
    return false;
  }

  if (pref_service_->GetBoolean(prefs::kNoProxyServer)) {
    // Ignore all the other proxy config preferences if the use of a proxy
    // has been explicitly disabled.
    return true;
  }

  if (pref_service_->HasPrefPath(prefs::kProxyServer)) {
    std::string proxy_server = pref_service_->GetString(prefs::kProxyServer);
    config->proxy_rules().ParseFromString(proxy_server);
  }

  if (pref_service_->HasPrefPath(prefs::kProxyPacUrl)) {
    std::string proxy_pac = pref_service_->GetString(prefs::kProxyPacUrl);
    config->set_pac_url(GURL(proxy_pac));
  }

  config->set_auto_detect(pref_service_->GetBoolean(prefs::kProxyAutoDetect));

  if (pref_service_->HasPrefPath(prefs::kProxyBypassList)) {
    std::string proxy_bypass =
        pref_service_->GetString(prefs::kProxyBypassList);
    config->proxy_rules().bypass_rules.ParseFromString(proxy_bypass);
  }

  return true;
}

PrefProxyConfigService::PrefProxyConfigService(
    PrefProxyConfigTracker* tracker,
    net::ProxyConfigService* base_service)
    : base_service_(base_service),
      pref_config_tracker_(tracker),
      registered_observers_(false) {
}

PrefProxyConfigService::~PrefProxyConfigService() {
  if (registered_observers_) {
    base_service_->RemoveObserver(this);
    pref_config_tracker_->RemoveObserver(this);
  }
}

void PrefProxyConfigService::AddObserver(
    net::ProxyConfigService::Observer* observer) {
  RegisterObservers();
  observers_.AddObserver(observer);
}

void PrefProxyConfigService::RemoveObserver(
    net::ProxyConfigService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrefProxyConfigService::GetLatestProxyConfig(net::ProxyConfig* config) {
  RegisterObservers();
  const net::ProxyConfig pref_config;
  if (pref_config_tracker_->GetProxyConfig(config))
    return true;

  return base_service_->GetLatestProxyConfig(config);
}

void PrefProxyConfigService::OnLazyPoll() {
  base_service_->OnLazyPoll();
}

void PrefProxyConfigService::OnProxyConfigChanged(
    const net::ProxyConfig& config) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Check whether there is a proxy configuration defined by preferences. In
  // this case that proxy configuration takes precedence and the change event
  // from the delegate proxy service can be disregarded.
  net::ProxyConfig pref_config;
  if (!pref_config_tracker_->GetProxyConfig(&pref_config)) {
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(config));
  }
}

void PrefProxyConfigService::OnPrefProxyConfigChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Evaluate the proxy configuration. If GetLatestProxyConfig returns false,
  // we are using the system proxy service, but it doesn't have a valid
  // configuration yet. Once it is ready, OnProxyConfigChanged() will be called
  // and broadcast the proxy configuration.
  // Note: If a switch between a preference proxy configuration and the system
  // proxy configuration occurs an unnecessary notification might get send if
  // the two configurations agree. This case should be rare however, so we don't
  // handle that case specially.
  net::ProxyConfig new_config;
  if (GetLatestProxyConfig(&new_config)) {
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(new_config));
  }
}

void PrefProxyConfigService::RegisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!registered_observers_) {
    base_service_->AddObserver(this);
    pref_config_tracker_->AddObserver(this);
    registered_observers_ = true;
  }
}
