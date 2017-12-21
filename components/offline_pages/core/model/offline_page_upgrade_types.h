// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_UPRGRADE_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_UPRGRADE_TYPES_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace offline_pages {

// Enumeration of possible results for starting the upgrade process.
enum class StartUpgradeStatus {
  SUCCESS,
  DB_ERROR,
  ITEM_MISSING,
  FILE_MISSING,
  NOT_ENOUGH_STORAGE,
};

// Result of starting the upgrade.
struct StartUpgradeResult {
  StartUpgradeResult();
  explicit StartUpgradeResult(StartUpgradeStatus status);
  StartUpgradeResult(StartUpgradeStatus status,
                     const std::string& digest,
                     const base::FilePath& file_path);

  // Support for move semantics.
  StartUpgradeResult(StartUpgradeResult&& other) = default;
  StartUpgradeResult& operator=(StartUpgradeResult&& other) = default;

  StartUpgradeStatus status;
  std::string digest;
  base::FilePath file_path;
};

// Callback delivering results of starting the upgrade.
typedef base::OnceCallback<void(StartUpgradeResult)> StartUpgradeCallback;

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_UPRGRADE_TYPES_H_
