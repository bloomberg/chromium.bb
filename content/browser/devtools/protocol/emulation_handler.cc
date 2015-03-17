// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/emulation_handler.h"

namespace content {
namespace devtools {
namespace emulation {

using Response = DevToolsProtocolClient::Response;

EmulationHandler::EmulationHandler() {
}

EmulationHandler::~EmulationHandler() {
}

void EmulationHandler::SetClient(scoped_ptr<DevToolsProtocolClient> client) {
}

Response EmulationHandler::SetGeolocationOverride(
    double* latitude, double* longitude, double* accuracy) {
  return Response::InternalError("Not implemented");
}

Response EmulationHandler::ClearGeolocationOverride() {
  return Response::InternalError("Not implemented");
}

Response EmulationHandler::SetTouchEmulationEnabled(
    bool enabled, const std::string* configuration) {
  return Response::InternalError("Not implemented");
}

Response EmulationHandler::CanEmulate(bool* result) {
  return Response::InternalError("Not implemented");
}

Response EmulationHandler::SetDeviceMetricsOverride(
    int width, int height, double device_scale_factor, bool mobile,
    bool fit_window, const double* optional_scale,
    const double* optional_offset_x, const double* optional_offset_y) {
  return Response::InternalError("Not implemented");
}

Response EmulationHandler::ClearDeviceMetricsOverride() {
  return Response::InternalError("Not implemented");
}

}  // namespace emulation
}  // namespace devtools
}  // namespace content
