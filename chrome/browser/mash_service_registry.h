// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MASH_SERVICE_REGISTRY_H_
#define CHROME_BROWSER_MASH_SERVICE_REGISTRY_H_

#include <string>

#include "content/public/browser/content_browser_client.h"

// Starts one of Mash's embedded services.
void RegisterOutOfProcessServicesForMash(
    content::ContentBrowserClient::OutOfProcessServiceMap* services);

// Returns true if |name| identifies a mash related service.
bool IsMashServiceName(const std::string& name);

#endif  // CHROME_BROWSER_MASH_SERVICE_REGISTRY_H_
