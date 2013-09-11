// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/dns_sd_registry.h"

namespace extensions {

DnsSdRegistry::DnsSdRegistry() {}
DnsSdRegistry::~DnsSdRegistry() {}

void DnsSdRegistry::AddObserver(DnsSdObserver* observer) {
  observers_.AddObserver(observer);
}

void DnsSdRegistry::RemoveObserver(DnsSdObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DnsSdRegistry::RegisterDnsSdListener(std::string service_type) {
  // TODO(mfoltz): Start DNS-SD service.
}

void DnsSdRegistry::UnregisterDnsSdListener(std::string service_type) {
  // TODO(mfoltz): Stop DNS-SD service.
}

}  // namespace extensions
