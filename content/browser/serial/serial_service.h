// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERIAL_SERIAL_SERVICE_H_
#define CONTENT_BROWSER_SERIAL_SERIAL_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

namespace content {

class RenderFrameHost;
class SerialChooser;

class SerialService : public blink::mojom::SerialService {
 public:
  explicit SerialService(RenderFrameHost* render_frame_host);
  ~SerialService() override;

  void Bind(blink::mojom::SerialServiceRequest request);

  // SerialService implementation
  void GetPorts(GetPortsCallback callback) override;
  void RequestPort(std::vector<blink::mojom::SerialPortFilterPtr> filters,
                   RequestPortCallback callback) override;

 private:
  RenderFrameHost* const render_frame_host_;
  mojo::BindingSet<blink::mojom::SerialService> bindings_;

  // The last shown serial port chooser UI.
  std::unique_ptr<SerialChooser> chooser_;

  DISALLOW_COPY_AND_ASSIGN(SerialService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERIAL_SERIAL_SERVICE_H_
