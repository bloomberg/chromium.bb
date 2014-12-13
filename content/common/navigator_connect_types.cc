// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigator_connect_types.h"

namespace content {

CrossOriginServiceWorkerClient::CrossOriginServiceWorkerClient()
    : message_port_id(-1) {
}

CrossOriginServiceWorkerClient::CrossOriginServiceWorkerClient(
    const GURL& target_url,
    const GURL& origin,
    int message_port_id)
    : target_url(target_url), origin(origin), message_port_id(message_port_id) {
}

CrossOriginServiceWorkerClient::~CrossOriginServiceWorkerClient() {
}

}  // namespace content
