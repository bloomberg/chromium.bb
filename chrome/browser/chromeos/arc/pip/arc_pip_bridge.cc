// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {

namespace {

// Singleton factory for ArcPipBridge.
class ArcPipBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPipBridge,
          ArcPipBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPipBridgeFactory";

  static ArcPipBridgeFactory* GetInstance() {
    return base::Singleton<ArcPipBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPipBridgeFactory>;

  ArcPipBridgeFactory() = default;
  ~ArcPipBridgeFactory() override = default;
};

}  // namespace

// static
ArcPipBridge* ArcPipBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPipBridgeFactory::GetForBrowserContext(context);
}

ArcPipBridge::ArcPipBridge(content::BrowserContext* context,
                           ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  DVLOG(2) << "ArcPipBridge::ArcPipBridge";
  arc_bridge_service_->pip()->SetHost(this);
  arc_bridge_service_->pip()->AddObserver(this);
}

ArcPipBridge::~ArcPipBridge() {
  DVLOG(2) << "ArcPipBridge::~ArcPipBridge";
  arc_bridge_service_->pip()->RemoveObserver(this);
  arc_bridge_service_->pip()->SetHost(nullptr);
}

void ArcPipBridge::OnConnectionReady() {
  DVLOG(1) << "ArcPipBridge::OnConnectionReady";
}

void ArcPipBridge::OnConnectionClosed() {
  DVLOG(1) << "ArcPipBridge::OnConnectionClosed";
}

void ArcPipBridge::OnPipEvent(arc::mojom::ArcPipEvent event) {
  DVLOG(1) << "ArcPipBridge::OnPipEvent";
  // TODO: Implement.
}

void ArcPipBridge::ClosePip() {
  DVLOG(1) << "ArcPipBridge::ClosePip";
  // TODO: Implement.
}

}  // namespace arc
