// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/tts.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// Provides text to speech services and events to Chrome OS via Android's text
// to speech API.
class ArcTtsService : public ArcService,
                      public InstanceHolder<mojom::TtsInstance>::Observer,
                      public mojom::TtsHost {
 public:
  explicit ArcTtsService(ArcBridgeService* bridge_service);
  ~ArcTtsService() override;

  // InstanceHolder<mojom::TtsInstance>::Observer overrides:
  void OnInstanceReady() override;

  // mojom::TtsHost overrides:
  void OnTtsEvent(uint32_t id,
                  mojom::TtsEventType event_type,
                  uint32_t char_index,
                  const std::string& error_msg) override;

 private:
  mojo::Binding<mojom::TtsHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcTtsService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TTS_ARC_TTS_SERVICE_H_
