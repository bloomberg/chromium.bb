// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_setup_flow.h"

#include "base/logging.h"

namespace local_discovery {

PrivetV3SetupFlow::Delegate::~Delegate() {
}

PrivetV3SetupFlow::PrivetV3SetupFlow(Delegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {
}

PrivetV3SetupFlow::~PrivetV3SetupFlow() {
}

void PrivetV3SetupFlow::Register(const std::string& service_name) {
  NOTIMPLEMENTED();
}

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
void PrivetV3SetupFlow::SetupWifiAndRegister(const std::string& device_ssid) {
  NOTIMPLEMENTED();
}
#endif  // ENABLE_WIFI_BOOTSTRAPPING

}  // namespace local_discovery
