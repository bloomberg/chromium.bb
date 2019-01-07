// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/serial/serial_service.h"

namespace content {

SerialService::SerialService(RenderFrameHost* render_frame_host) {}

SerialService::~SerialService() = default;

void SerialService::Bind(blink::mojom::SerialServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void SerialService::GetPorts(GetPortsCallback callback) {
  std::move(callback).Run(std::vector<blink::mojom::SerialPortInfoPtr>());
}

void SerialService::RequestPort(
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    RequestPortCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace content
