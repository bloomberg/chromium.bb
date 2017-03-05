// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/ime_info.h"

namespace ash {

IMEInfo::IMEInfo() : selected(false), third_party(false) {}

IMEInfo::IMEInfo(const IMEInfo& other) = default;

IMEInfo::~IMEInfo() {}

IMEPropertyInfo::IMEPropertyInfo() : selected(false) {}

IMEPropertyInfo::~IMEPropertyInfo() {}

}  // namespace ash
