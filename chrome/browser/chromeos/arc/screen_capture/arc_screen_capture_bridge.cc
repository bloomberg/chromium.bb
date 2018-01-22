// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/screen_capture/arc_screen_capture_bridge.h"

#include "base/memory/singleton.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {
namespace {

// Singleton factory for ArcScreenCaptureBridge
class ArcScreenCaptureBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcScreenCaptureBridge,
          ArcScreenCaptureBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcScreenCaptureBridgeFactory";

  static ArcScreenCaptureBridgeFactory* GetInstance() {
    return base::Singleton<ArcScreenCaptureBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcScreenCaptureBridgeFactory>;
  ArcScreenCaptureBridgeFactory() = default;
  ~ArcScreenCaptureBridgeFactory() override = default;
};

}  // namespace

// static
ArcScreenCaptureBridge* ArcScreenCaptureBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcScreenCaptureBridgeFactory::GetForBrowserContext(context);
}

ArcScreenCaptureBridge::ArcScreenCaptureBridge(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), weak_factory_(this) {
  arc_bridge_service_->screen_capture()->SetHost(this);
}

ArcScreenCaptureBridge::~ArcScreenCaptureBridge() {
  arc_bridge_service_->screen_capture()->SetHost(nullptr);
}

void ArcScreenCaptureBridge::RequestPermission(
    const std::string& display_name,
    const std::string& package_name,
    RequestPermissionCallback callback) {
  NOTREACHED();
}

void ArcScreenCaptureBridge::OpenSession(
    mojom::ScreenCaptureSessionNotifierPtr notifier,
    const std::string& package_name,
    const gfx::Size& size,
    OpenSessionCallback callback) {
  NOTREACHED();
}

}  // namespace arc
