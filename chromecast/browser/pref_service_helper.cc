// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/pref_service_helper.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/pref_store.h"
#include "chromecast/common/cast_paths.h"
#include "chromecast/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace chromecast {
namespace shell {

namespace {

void UserPrefsLoadError(PersistentPrefStore::PrefReadError* error_val,
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
scoped_ptr<PrefService> PrefServiceHelper::CreatePrefService(
    PrefRegistrySimple* registry) {
  const base::FilePath config_path(GetConfigPath());
  VLOG(1) << "Loading config from " << config_path.value();

  registry->RegisterBooleanPref(prefs::kMetricsIsNewClientID, false);
  registry->RegisterIntegerPref(prefs::kRemoteDebuggingPort, 0);

  RegisterPlatformPrefs(registry);

  base::PrefServiceFactory prefServiceFactory;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      JsonPrefStore::GetTaskRunnerForFile(
          config_path,
          content::BrowserThread::GetBlockingPool());
  prefServiceFactory.SetUserPrefsFile(config_path, task_runner.get());
  prefServiceFactory.set_async(false);

  PersistentPrefStore::PrefReadError prefs_read_error =
      PersistentPrefStore::PREF_READ_ERROR_NONE;
  prefServiceFactory.set_read_error_callback(
      base::Bind(&UserPrefsLoadError, &prefs_read_error));

  scoped_ptr<PrefService> pref_service(prefServiceFactory.Create(registry));
  if (prefs_read_error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "Cannot initialize chromecast config: "
               << config_path.value()
               << ", pref_error=" << prefs_read_error;
  }

  OnPrefsLoaded(pref_service.get());
  return pref_service.Pass();
}

}  // namespace shell
}  // namespace chromecast
