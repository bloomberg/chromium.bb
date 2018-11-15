// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon_purger.h"

#include "ash/system/network/network_icon.h"
#include "base/bind.h"
#include "chromeos/network/network_state_handler.h"

using chromeos::NetworkHandler;

namespace {
const int kPurgeDelayMs = 300;
}  // namespace

namespace ash {

NetworkIconPurger::NetworkIconPurger() {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    auto* network_handler = NetworkHandler::Get();
    network_handler->network_state_handler()->AddObserver(this, FROM_HERE);
  }
}

NetworkIconPurger::~NetworkIconPurger() {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    auto* network_handler = NetworkHandler::Get();
    network_handler->network_state_handler()->RemoveObserver(this, FROM_HERE);
  }
}

void NetworkIconPurger::NetworkListChanged() {
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kPurgeDelayMs),
               base::BindOnce(&network_icon::PurgeNetworkIconCache));
}

}  // namespace ash
