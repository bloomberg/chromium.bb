// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_service.h"

#include <utility>

namespace content {

HidService::HidService() = default;

HidService::~HidService() = default;

void HidService::Bind(blink::mojom::HidServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace content
