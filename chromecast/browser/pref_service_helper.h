// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper to initialize PrefService for cast shell.

#ifndef CHROMECAST_BROWSER_PREF_SERVICE_HELPER_H_
#define CHROMECAST_BROWSER_PREF_SERVICE_HELPER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;

namespace chromecast {
namespace shell {

// It uses JsonPrefStore internally and/so the format of config file is same to
// that of JsonPrefStore.
class PrefServiceHelper {
 public:
  // Loads configs from config file. Returns true if successful.
  static std::unique_ptr<PrefService> CreatePrefService(
      PrefRegistrySimple* registry);

 private:
  // Registers any needed preferences for the current platform.
  static void RegisterPlatformPrefs(PrefRegistrySimple* registry);

  // Called after the pref file has been loaded.
  static void OnPrefsLoaded(PrefService* pref_service);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_PREF_SERVICE_HELPER_H_
