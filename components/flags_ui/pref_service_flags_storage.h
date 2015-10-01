// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_
#define COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_

#include "base/compiler_specific.h"
#include "components/flags_ui/flags_storage.h"

class PrefService;

namespace flags_ui {

// Implements the FlagsStorage interface with a PrefService backend.
// This is the implementation used on desktop Chrome for all users.
class PrefServiceFlagsStorage : public FlagsStorage {
 public:
  explicit PrefServiceFlagsStorage(PrefService* prefs);
  ~PrefServiceFlagsStorage() override;

  std::set<std::string> GetFlags() override;
  bool SetFlags(const std::set<std::string>& flags) override;

 private:
  PrefService* prefs_;
};

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_PREF_SERVICE_FLAGS_STORAGE_H_
