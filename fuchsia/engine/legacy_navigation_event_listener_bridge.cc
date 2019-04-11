// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/legacy_navigation_event_listener_bridge.h"

#include "base/fuchsia/fuchsia_logging.h"

namespace {

chromium::web::NavigationEvent ConvertToLegacyNavigationEvent(
    const fuchsia::web::NavigationState& entry) {
  chromium::web::NavigationEvent converted;

  if (entry.has_title())
    converted.title = entry.title();
  if (entry.has_url())
    converted.url = entry.url();
  if (entry.has_page_type())
    converted.is_error = entry.page_type() == fuchsia::web::PageType::ERROR;

  return converted;
}

}  // namespace

LegacyNavigationEventListenerBridge::LegacyNavigationEventListenerBridge(
    fidl::InterfaceRequest<fuchsia::web::NavigationEventListener> request,
    chromium::web::NavigationEventObserverPtr handle)
    : binding_(this, std::move(request)), observer_(std::move(handle)) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |binding_| disconnected.";
    delete this;
  });
  observer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |observer_| disconnected.";
    delete this;
  });
}

LegacyNavigationEventListenerBridge::~LegacyNavigationEventListenerBridge() =
    default;

void LegacyNavigationEventListenerBridge::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  observer_->OnNavigationStateChanged(ConvertToLegacyNavigationEvent(change),
                                      std::move(callback));
  // [callback = std::move(callback)]() { callback(); });
}
