// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/prerender_condition_network.h"

#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

PrerenderConditionNetwork::PrerenderConditionNetwork() {
}

PrerenderConditionNetwork::~PrerenderConditionNetwork() {
}

bool PrerenderConditionNetwork::CanPrerender() const {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  return default_network &&
         !default_network->Matches(NetworkTypePattern::Mobile());
}

}  // namespace chromeos
