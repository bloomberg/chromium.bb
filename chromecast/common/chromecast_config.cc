// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/chromecast_config.h"

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/common/cast_paths.h"
#include "chromecast/common/pref_names.h"

namespace chromecast {

namespace {

// Config file IO worker constants.
const int kNumOfConfigFileIOWorkers = 1;
const char kNameOfConfigFileIOWorkers[] = "ConfigFileIO";

void UserPrefsLoadError(
    PersistentPrefStore::PrefReadError* error_val,
    PersistentPrefStore::PrefReadError error) {
  DCHECK(error_val);
  *error_val = error;
}

base::FilePath GetConfigPath() {
  base::FilePath config_path;
  CHECK(PathService::Get(FILE_CAST_CONFIG, &config_path));
  return config_path;
}

}  // namespace

// static
ChromecastConfig* ChromecastConfig::g_instance_ = NULL;

// static
void ChromecastConfig::Create(PrefRegistrySimple* registry) {
  DCHECK(g_instance_ == NULL);
  g_instance_ = new ChromecastConfig();
  g_instance_->Load(registry);
}

// static
ChromecastConfig* ChromecastConfig::GetInstance() {
  DCHECK(g_instance_ != NULL);
  return g_instance_;
}

ChromecastConfig::ChromecastConfig()
    : config_path_(GetConfigPath()),
      worker_pool_(new base::SequencedWorkerPool(kNumOfConfigFileIOWorkers,
                                                 kNameOfConfigFileIOWorkers)) {
}

ChromecastConfig::~ChromecastConfig() {
  // Explict writing before worker_pool shutdown.
  pref_service_->CommitPendingWrite();
  worker_pool_->Shutdown();
}

bool ChromecastConfig::Load(PrefRegistrySimple* registry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Loading config from " << config_path_.value();
  registry->RegisterIntegerPref(prefs::kRemoteDebuggingPort, 0);

  RegisterPlatformPrefs(registry);

  PersistentPrefStore::PrefReadError prefs_read_error =
      PersistentPrefStore::PREF_READ_ERROR_NONE;
  base::PrefServiceFactory prefServiceFactory;
  prefServiceFactory.SetUserPrefsFile(config_path_,
      JsonPrefStore::GetTaskRunnerForFile(config_path_, worker_pool_));
  prefServiceFactory.set_async(false);
  prefServiceFactory.set_read_error_callback(
      base::Bind(&UserPrefsLoadError, &prefs_read_error));
  pref_service_ = prefServiceFactory.Create(registry);

  if (prefs_read_error == PersistentPrefStore::PREF_READ_ERROR_NONE) {
    return true;
  } else {
    LOG(ERROR) << "Cannot initialize chromecast config: "
               << config_path_.value()
               << ", pref_error=" << prefs_read_error;
    return false;
  }
}

void ChromecastConfig::Save() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Saving config to: " << config_path_.value();
  pref_service_->CommitPendingWrite();
}

const std::string ChromecastConfig::GetValue(const std::string& key) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return pref_service_->GetString(key.c_str());
}

const int ChromecastConfig::GetIntValue(const std::string& key) const {
  return pref_service_->GetInteger(key.c_str());
}

void ChromecastConfig::SetValue(
    const std::string& key,
    const std::string& value) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pref_service_->IsUserModifiablePreference(key.c_str())) {
    VLOG(1) << "Set config: key=" << key << ", value=" << value;
    pref_service_->SetString(key.c_str(), value);
  } else {
    LOG(ERROR) << "Cannot set read-only config: key=" << key
               << ", value=" << value;
  }
}

void ChromecastConfig::SetIntValue(const std::string& key, int value) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pref_service_->IsUserModifiablePreference(key.c_str())) {
    VLOG(1) << "Set config: key=" << key << ", value=" << value;
    pref_service_->SetInteger(key.c_str(), value);
  } else {
    LOG(ERROR) << "Cannot set read-only config: key=" << key
               << ", value=" << value;
  }
}

}  // namespace chromecast
