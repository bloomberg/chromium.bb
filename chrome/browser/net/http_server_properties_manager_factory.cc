// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_server_properties_manager_factory.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_server_properties_manager.h"

namespace {

// Connects the HttpServerPropertiesManager's storage to the Chrome prefs.
class PrefServiceAdapter
    : public net::HttpServerPropertiesManager::PrefDelegate {
 public:
  explicit PrefServiceAdapter(PrefService* pref_service)
      : pref_service_(pref_service),
        path_(prefs::kHttpServerProperties) {
    pref_change_registrar_.Init(pref_service_);
  }

  ~PrefServiceAdapter() override {
  }

  // PrefDelegate implementation.
  bool HasServerProperties() override {
    return pref_service_->HasPrefPath(path_);
  }
  const base::DictionaryValue& GetServerProperties() const override {
    // Guaranteed not to return null when the pref is registered
    // (RegisterProfilePrefs was called).
    return *pref_service_->GetDictionary(path_);
  }
  void SetServerProperties(const base::DictionaryValue& value) override {
    return pref_service_->Set(path_, value);
  }
  void StartListeningForUpdates(const base::Closure& callback) override {
    pref_change_registrar_.Add(path_, callback);
  }
  void StopListeningForUpdates() override {
    pref_change_registrar_.RemoveAll();
  }

 private:
  PrefService* pref_service_;
  const std::string path_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceAdapter);
};

}  // namespace

namespace chrome_browser_net {

/* static */
net::HttpServerPropertiesManager*
HttpServerPropertiesManagerFactory::CreateManager(PrefService* pref_service,
                                                  net::NetLog* net_log) {
  using content::BrowserThread;
  return new net::HttpServerPropertiesManager(
      new PrefServiceAdapter(pref_service),  // Transfers ownership.
      base::ThreadTaskRunnerHandle::Get(),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO), net_log);
}

/* static */
void HttpServerPropertiesManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kHttpServerProperties);
}

}  // namespace chrome_browser_net
