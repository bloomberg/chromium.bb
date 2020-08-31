// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_metadata_observer.h"

namespace chromeos {

NetworkMetadataObserver::NetworkMetadataObserver() = default;

NetworkMetadataObserver::~NetworkMetadataObserver() = default;

void NetworkMetadataObserver::OnFirstConnectionToNetwork(
    const std::string& guid) {}

void NetworkMetadataObserver::OnNetworkUpdate(
    const std::string& guid,
    base::DictionaryValue* set_properties) {}

}  // namespace chromeos
