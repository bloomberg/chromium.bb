// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MASH_SERVICE_REGISTRY_H_
#define CHROME_BROWSER_MASH_SERVICE_REGISTRY_H_

#include <string>

#include "content/public/browser/content_browser_client.h"

namespace mash_service_registry {

// Process group used for the ash service and the ui service. Visible for test.
constexpr char kAshAndUiProcessGroup[] = "ash_and_ui";

// Starts one of Mash's embedded services.
void RegisterOutOfProcessServices(
    content::ContentBrowserClient::OutOfProcessServiceMap* services);

// Returns true if |name| identifies a mash related service.
bool IsMashServiceName(const std::string& name);

// If |service_name| identifies a mash related service returns an arbitrary
// label that identifies the service or group of related services. Otherwise
// returns the empty string.
std::string GetMashServiceLabel(const std::string& service_name);

// Returns true if the browser should exit when service |name| quits.
bool ShouldTerminateOnServiceQuit(const std::string& name);

}  // namespace mash_service_registry

#endif  // CHROME_BROWSER_MASH_SERVICE_REGISTRY_H_
