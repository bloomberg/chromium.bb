// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/prerender_condition_network.h"

namespace chromeos {

PrerenderConditionNetwork::PrerenderConditionNetwork(
    NetworkLibrary* network_library) : network_library_(network_library) {
  DCHECK(network_library_);
}

PrerenderConditionNetwork::~PrerenderConditionNetwork() {
}

bool PrerenderConditionNetwork::CanPrerender() const {
  const Network* active_network = network_library_->active_network();
  if (!active_network)
    return false;
  switch (active_network->type()) {
    case TYPE_ETHERNET:
    case TYPE_WIFI:
      return true;
    default:
      return false;
  }
}

}  // namespace chromeos
