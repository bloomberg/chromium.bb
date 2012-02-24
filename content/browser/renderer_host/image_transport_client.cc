// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_client.h"

ImageTransportClient::ImageTransportClient(bool flipped, const gfx::Size& size)
    : ui::Texture(flipped, size) {
}
