// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_zoom_request_client.h"

namespace extensions {

ExtensionZoomRequestClient::ExtensionZoomRequestClient(
    scoped_refptr<const Extension> extension)
    : extension_(extension) {
}

ExtensionZoomRequestClient::~ExtensionZoomRequestClient() {
}

}  // namespace Extensions
