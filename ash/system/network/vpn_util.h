// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_VPN_UTIL_H_
#define ASH_SYSTEM_NETWORK_VPN_UTIL_H_

namespace ash {
namespace vpn_util {

extern bool IsVPNVisibleInSystemTray();
extern bool IsVPNEnabled();
extern bool IsVPNConnected();

}  // namespace vpn_util
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_VPN_UTIL_H_
