// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
#define ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
#pragma once

namespace ash {

struct NetworkIconInfo;

class NetworkObserver {
 public:
  virtual ~NetworkObserver() {}

  virtual void OnNetworkRefresh(const NetworkIconInfo& info) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H
