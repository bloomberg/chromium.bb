// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/network_info.h"

namespace ash {

NetworkInfo::NetworkInfo()
    : disable(false),
      highlight(false),
      connected(false),
      connecting(false),
      type(Type::UNKNOWN) {}

NetworkInfo::NetworkInfo(const std::string& guid)
    : guid(guid),
      disable(false),
      highlight(false),
      connected(false),
      connecting(false),
      type(Type::UNKNOWN) {}

NetworkInfo::~NetworkInfo() = default;

bool NetworkInfo::operator==(const NetworkInfo& other) const {
  return guid == other.guid && label == other.label &&
         tooltip == other.tooltip && image.BackedBySameObjectAs(other.image) &&
         disable == other.disable && highlight == other.highlight &&
         connected == other.connected && connecting == other.connecting &&
         type == other.type;
}

}  // namespace ash
