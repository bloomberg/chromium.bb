// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_types.h"

#include "base/logging.h"

namespace ash {

bool IsValidShelfItemType(int64_t type) {
  return type == TYPE_APP_PANEL || type == TYPE_PINNED_APP ||
         type == TYPE_APP_LIST || type == TYPE_BROWSER_SHORTCUT ||
         type == TYPE_APP || type == TYPE_DIALOG || type == TYPE_UNDEFINED;
}

ShelfID::ShelfID(const std::string& app_id, const std::string& launch_id)
    : app_id(app_id), launch_id(launch_id) {
  DCHECK(launch_id.empty() || !app_id.empty()) << "launch ids require app ids.";
}

ShelfID::~ShelfID() = default;

ShelfID::ShelfID(const ShelfID& other) = default;

ShelfID::ShelfID(ShelfID&& other) = default;

ShelfID& ShelfID::operator=(const ShelfID& other) = default;

bool ShelfID::operator==(const ShelfID& other) const {
  return app_id == other.app_id && launch_id == other.launch_id;
}

bool ShelfID::operator!=(const ShelfID& other) const {
  return !(*this == other);
}

bool ShelfID::operator<(const ShelfID& other) const {
  return app_id < other.app_id ||
         (app_id == other.app_id && launch_id < other.launch_id);
}

bool ShelfID::IsNull() const {
  return app_id.empty() && launch_id.empty();
}

}  // namespace ash
