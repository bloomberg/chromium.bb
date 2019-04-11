// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_LEGACY_NAVIGATION_CONTROLLER_BRIDGE_H_
#define FUCHSIA_ENGINE_LEGACY_NAVIGATION_CONTROLLER_BRIDGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <string>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

// Allows chromium::web::gNavigationControllerePort clients to connect to
// fuchsia::web::NavigationController instances.
//
// LegacyNavigationControllerBridge instances are self-managed; they destroy
// themselves when the connection with either end is terminated.
class LegacyNavigationControllerBridge
    : public chromium::web::NavigationController {
 public:
  LegacyNavigationControllerBridge(
      fidl::InterfaceRequest<chromium::web::NavigationController> request,
      fuchsia::web::NavigationControllerPtr handle);

 private:
  // Non-public to ensure that only this object may destroy itself.
  ~LegacyNavigationControllerBridge() override;

  // chromium::web::NavigationController implementation.
  void LoadUrl(std::string url, chromium::web::LoadUrlParams params) override;
  void GoBack() override;
  void GoForward() override;
  void Stop() override;
  void Reload(chromium::web::ReloadType type) override;
  void GetVisibleEntry(GetVisibleEntryCallback callback) override;

  fidl::Binding<chromium::web::NavigationController> binding_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;

  DISALLOW_COPY_AND_ASSIGN(LegacyNavigationControllerBridge);
};

#endif  // FUCHSIA_ENGINE_LEGACY_NAVIGATION_CONTROLLER_BRIDGE_H_
