// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {

namespace {

// Singleton factory for ArcInputMethodManagerService
class ArcInputMethodManagerServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputMethodManagerService,
          ArcInputMethodManagerServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase
  static constexpr const char* kName = "ArcInputMethodManagerServiceFactory";

  static ArcInputMethodManagerServiceFactory* GetInstance() {
    return base::Singleton<ArcInputMethodManagerServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcInputMethodManagerServiceFactory>;
  ArcInputMethodManagerServiceFactory() = default;
  ~ArcInputMethodManagerServiceFactory() override = default;
};

}  // namespace

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContext(context);
}

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContextForTesting(
      context);
}

ArcInputMethodManagerService::ArcInputMethodManagerService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : imm_bridge_(
          std::make_unique<ArcInputMethodManagerBridgeImpl>(this,
                                                            bridge_service)) {}

ArcInputMethodManagerService::~ArcInputMethodManagerService() = default;

void ArcInputMethodManagerService::SetInputMethodManagerBridgeForTesting(
    std::unique_ptr<ArcInputMethodManagerBridge> test_bridge) {
  imm_bridge_ = std::move(test_bridge);
}

void ArcInputMethodManagerService::OnActiveImeChanged(
    const std::string& ime_id) {
  // Please see https://crbug.com/845079.
  NOTIMPLEMENTED();
}

void ArcInputMethodManagerService::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
  // Please see https://crbug.com/845079.
  NOTIMPLEMENTED();
}

}  // namespace arc
