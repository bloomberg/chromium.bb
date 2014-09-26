// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Chromecast-specific configurations retrieved from and stored into a given
// configuration file.

#ifndef CHROMECAST_COMMON_CHROMECAST_CONFIG_H_
#define CHROMECAST_COMMON_CHROMECAST_CONFIG_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"

class PrefRegistrySimple;

namespace chromecast {

// Manages configuration for Chromecast via PrefService.
// It uses JsonPrefStore internally and/so the format of config file is same to
// that of JsonPrefStore.
// It is NOT thread-safe; all functions must be run on the same thread as
// the object is created.
class ChromecastConfig {
 public:
  // Creates new singleton instance of ChromecastConfig.
  static void Create(PrefRegistrySimple* registry);

  // Returns the singleton instance of ChromecastConfig.
  static ChromecastConfig* GetInstance();

  // Saves configs into configuration file.
  void Save() const;

  // Returns string value for |key|, if present.
  const std::string GetValue(const std::string& key) const;

  // Returns integer value for |key|, if present.
  const int GetIntValue(const std::string& key) const;

  // Sets new string value for |key|.
  void SetValue(const std::string& key, const std::string& value) const;

  // Sets new int value for |key|.
  void SetIntValue(const std::string& key, int value) const;

  // Whether or not a value has been set for |key|.
  bool HasValue(const std::string& key) const;

  scoped_refptr<base::SequencedWorkerPool> worker_pool() const {
    return worker_pool_;
  }

  PrefService* pref_service() const { return pref_service_.get(); }

 private:
  ChromecastConfig();
  ~ChromecastConfig();

  // Loads configs from config file. Returns true if successful.
  bool Load(PrefRegistrySimple* registry);

  // Registers any needed preferences for the current platform.
  void RegisterPlatformPrefs(PrefRegistrySimple* registry);

  // Global singleton instance.
  static ChromecastConfig* g_instance_;

  const base::FilePath config_path_;
  const scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_ptr<PrefService> pref_service_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ChromecastConfig);
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CHROMECAST_CONFIG_H_
