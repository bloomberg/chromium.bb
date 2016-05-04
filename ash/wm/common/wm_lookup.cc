// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/wm_lookup.h"

namespace ash {
namespace wm {

// static
WmLookup* WmLookup::instance_ = nullptr;

// static
void WmLookup::Set(WmLookup* lookup) {
  instance_ = lookup;
}

// static
WmLookup* WmLookup::Get() {
  return instance_;
}

}  // namespace wm
}  // namespace ash
