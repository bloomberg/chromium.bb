// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mock_network_library.h"

namespace chromeos {

MockNetworkLibrary::MockNetworkLibrary() {}

MockNetworkLibrary::~MockNetworkLibrary() {}

MockCellularNetwork::MockCellularNetwork(const std::string& service_path)
    : CellularNetwork(service_path) {}

MockCellularNetwork::~MockCellularNetwork() {}

}  // namespace chromeos
