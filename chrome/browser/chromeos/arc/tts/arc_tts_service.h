// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/common/tts.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

class BrowserContextKeyedServiceFactory;
class TtsController;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Provides text to speech services and events to Chrome OS via Android's text
// to speech API.
class ArcTtsService : public KeyedService,
                      public InstanceHolder<mojom::TtsInstance>::Observer,
                      public mojom::TtsHost {
 public:
  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcTtsService* GetForBrowserContext(content::BrowserContext* context);

  ArcTtsService(content::BrowserContext* context,
                ArcBridgeService* bridge_service);
  ~ArcTtsService() override;

  // InstanceHolder<mojom::TtsInstance>::Observer overrides:
  void OnInstanceReady() override;

  // mojom::TtsHost overrides:
  void OnTtsEvent(uint32_t id,
                  mojom::TtsEventType event_type,
                  uint32_t char_index,
                  const std::string& error_msg) override;

  void set_tts_controller_for_testing(TtsController* tts_controller) {
    tts_controller_ = tts_controller;
  }

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  mojo::Binding<mojom::TtsHost> binding_;
  TtsController* tts_controller_;

  DISALLOW_COPY_AND_ASSIGN(ArcTtsService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_
