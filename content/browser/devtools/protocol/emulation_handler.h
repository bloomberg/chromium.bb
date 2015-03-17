// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

namespace content {
namespace devtools {
namespace emulation {

class EmulationHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  EmulationHandler();
  virtual ~EmulationHandler();

  Response SetGeolocationOverride(double* latitude,
                                  double* longitude,
                                  double* accuracy);
  Response ClearGeolocationOverride();

  Response SetTouchEmulationEnabled(bool enabled,
                                    const std::string* configuration);

  Response CanEmulate(bool* result);
  Response SetDeviceMetricsOverride(int width,
                                    int height,
                                    double device_scale_factor,
                                    bool mobile,
                                    bool fit_window,
                                    const double* optional_scale,
                                    const double* optional_offset_x,
                                    const double* optional_offset_y);
  Response ClearDeviceMetricsOverride();

  void SetClient(scoped_ptr<DevToolsProtocolClient> client);

 private:
  DISALLOW_COPY_AND_ASSIGN(EmulationHandler);
};

}  // namespace emulation
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
