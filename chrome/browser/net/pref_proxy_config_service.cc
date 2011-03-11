// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/pref_proxy_config_service.h"

#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

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
    PrefProxyConfigTracker::Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.AddObserver(observer);
}

void PrefProxyConfigTracker::RemoveObserver(
    PrefProxyConfigTracker::Observer* observer) {
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
  if (valid_ != valid || (valid && !pref_config_.Equals(config))) {
    valid_ = valid;
    if (valid_)
      pref_config_ = config;
    FOR_EACH_OBSERVER(Observer, observers_, OnPrefProxyConfigChanged());
  }
}

bool PrefProxyConfigTracker::ReadPrefConfig(net::ProxyConfig* config) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Clear the configuration.
  *config = net::ProxyConfig();

  const DictionaryValue* dict = pref_service_->GetDictionary(prefs::kProxy);
  DCHECK(dict);
  ProxyConfigDictionary proxy_dict(dict);

  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict.GetMode(&mode)) {
    // Fall back to system settings if the mode preference is invalid.
    return false;
  }

  switch (mode) {
    case ProxyPrefs::MODE_SYSTEM:
      // Use system settings.
      return false;
    case ProxyPrefs::MODE_DIRECT:
      // Ignore all the other proxy config preferences if the use of a proxy
      // has been explicitly disabled.
      return true;
    case ProxyPrefs::MODE_AUTO_DETECT:
      config->set_auto_detect(true);
      return true;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string proxy_pac;
      if (!proxy_dict.GetPacUrl(&proxy_pac)) {
        LOG(ERROR) << "Proxy settings request PAC script but do not specify "
                   << "its URL. Falling back to direct connection.";
        return true;
      }
      GURL proxy_pac_url(proxy_pac);
      if (!proxy_pac_url.is_valid()) {
        LOG(ERROR) << "Invalid proxy PAC url: " << proxy_pac;
        return true;
      }
      config->set_pac_url(proxy_pac_url);
      return true;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      if (!proxy_dict.GetProxyServer(&proxy_server)) {
        LOG(ERROR) << "Proxy settings request fixed proxy servers but do not "
                   << "specify their URLs. Falling back to direct connection.";
        return true;
      }
      config->proxy_rules().ParseFromString(proxy_server);

      std::string proxy_bypass;
      if (proxy_dict.GetBypassList(&proxy_bypass)) {
        config->proxy_rules().bypass_rules.ParseFromString(proxy_bypass);
      }
      return true;
    }
    case ProxyPrefs::kModeCount: {
      // Fall through to NOTREACHED().
    }
  }
  NOTREACHED() << "Unknown proxy mode, falling back to system settings.";
  return false;
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
  net::ProxyConfig pref_config;
  if (pref_config_tracker_->GetProxyConfig(&pref_config)) {
    *config = pref_config;
    return true;
  }

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

// static
void PrefProxyConfigService::RegisterPrefs(PrefService* pref_service) {
  DictionaryValue* default_settings = ProxyConfigDictionary::CreateSystem();
  pref_service->RegisterDictionaryPref(prefs::kProxy, default_settings);
}
