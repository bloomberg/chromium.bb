// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon_purger.h"

#include "ash/system/network/network_icon.h"
#include "base/bind.h"
#include "chromeos/network/network_state_handler.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;

namespace ash {

namespace {

const int kPurgeDelayMs = 300;

void PurgeNetworkIconCache() {
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkList(
      &networks);
  std::set<std::string> network_paths;
  for (NetworkStateHandler::NetworkStateList::iterator iter = networks.begin();
       iter != networks.end(); ++iter) {
    network_paths.insert((*iter)->path());
  }
  network_icon::PurgeNetworkIconCache(network_paths);
}

}  // namespace

NetworkIconPurger::NetworkIconPurger() {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
}

NetworkIconPurger::~NetworkIconPurger() {
  // NetworkHandler may not be initialized in tests.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void NetworkIconPurger::NetworkListChanged() {
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kPurgeDelayMs),
               base::BindOnce(&PurgeNetworkIconCache));
}

}  // namespace ash
