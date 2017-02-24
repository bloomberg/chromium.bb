// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_types.h"

namespace ash {

bool IsValidShelfItemType(int64_t type) {
  return type == TYPE_APP_PANEL || type == TYPE_APP_SHORTCUT ||
         type == TYPE_APP_LIST || type == TYPE_BROWSER_SHORTCUT ||
         type == TYPE_APP || type == TYPE_DIALOG || type == TYPE_UNDEFINED;
}

}  // namespace ash
