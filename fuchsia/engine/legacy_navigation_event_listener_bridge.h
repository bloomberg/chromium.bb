// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_LEGACY_NAVIGATION_EVENT_LISTENER_BRIDGE_H_
#define FUCHSIA_ENGINE_LEGACY_NAVIGATION_EVENT_LISTENER_BRIDGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

// Allows fuchsia::web::NavigationEventListener clients to connect to
// chromium::web::NavigationEventObserver instances.
//
// LegacyNavigationEventListenerBridge instances are self-managed; they destroy
// themselves when the connection with either end is terminated.
class LegacyNavigationEventListenerBridge
    : public fuchsia::web::NavigationEventListener {
 public:
  LegacyNavigationEventListenerBridge(
      fidl::InterfaceRequest<fuchsia::web::NavigationEventListener> request,
      chromium::web::NavigationEventObserverPtr handle);

 private:
  // Non-public to ensure that only this object may destroy itself.
  ~LegacyNavigationEventListenerBridge() override;

  // fuchsia::web::NavigationEventListener implementation.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  fidl::Binding<fuchsia::web::NavigationEventListener> binding_;
  chromium::web::NavigationEventObserverPtr observer_;

  DISALLOW_COPY_AND_ASSIGN(LegacyNavigationEventListenerBridge);
};

#endif  // FUCHSIA_ENGINE_LEGACY_NAVIGATION_EVENT_LISTENER_BRIDGE_H_
