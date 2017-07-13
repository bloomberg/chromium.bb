// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_

#include "content/common/content_export.h"

namespace content {

namespace mojom {
class NetworkService;
}

// Returns a pointer to the NetworkService, creating / re-creating it as needed.
// Must only be called on the UI thread. Must not be called if the network
// service is disabled.
CONTENT_EXPORT mojom::NetworkService* GetNetworkService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_