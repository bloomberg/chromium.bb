// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/legacy_context_bridge.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/engine/legacy_frame_bridge.h"

LegacyContextBridge::LegacyContextBridge(
    fidl::InterfaceRequest<chromium::web::Context> request,
    fuchsia::web::ContextPtr handle)
    : binding_(this, std::move(request)), context_(std::move(handle)) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |binding_| disconnected.";
    delete this;
  });
  context_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |context_| disconnected.";
    delete this;
  });
}

LegacyContextBridge::~LegacyContextBridge() = default;

void LegacyContextBridge::CreateFrame(
    fidl::InterfaceRequest<chromium::web::Frame> frame) {
  fuchsia::web::FramePtr fuchsia_frame;
  context_->CreateFrame(fuchsia_frame.NewRequest());
  new LegacyFrameBridge(std::move(frame), std::move(fuchsia_frame));
}
