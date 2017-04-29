// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_base.h"

#include "base/logging.h"

namespace ash {

NetworkListViewBase::NetworkListViewBase(
    tray::NetworkStateListDetailedView* detailed_view)
    : detailed_view_(detailed_view) {
  DCHECK(detailed_view_);
}

NetworkListViewBase::~NetworkListViewBase() {}

}  // namespace ash
