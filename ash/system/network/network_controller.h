// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_CONTROLLER_H
#define ASH_SYSTEM_NETWORK_NETWORK_CONTROLLER_H
#pragma once

namespace ash {

struct NetworkIconInfo;

class NetworkController {
 public:
  virtual ~NetworkController() {}

  virtual void OnNetworkRefresh(const NetworkIconInfo& info) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_CONTROLLER_H
