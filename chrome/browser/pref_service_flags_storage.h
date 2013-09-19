// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREF_SERVICE_FLAGS_STORAGE_H_
#define CHROME_BROWSER_PREF_SERVICE_FLAGS_STORAGE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/flags_storage.h"

class PrefService;

namespace about_flags {

// Implements the FlagsStorage interface with a PrefService backend.
// This is the implementation used on desktop Chrome for all users.
class PrefServiceFlagsStorage : public FlagsStorage {
 public:
  explicit PrefServiceFlagsStorage(PrefService *prefs);
  virtual ~PrefServiceFlagsStorage();

  virtual std::set<std::string> GetFlags() OVERRIDE;
  virtual bool SetFlags(const std::set<std::string>& flags) OVERRIDE;

 private:
  PrefService* prefs_;
};

}  // namespace about_flags

#endif  // CHROME_BROWSER_PREF_SERVICE_FLAGS_STORAGE_H_
