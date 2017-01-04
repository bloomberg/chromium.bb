// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tts/arc_tts_service.h"

#include "base/logging.h"
#include "chrome/browser/speech/tts_controller.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcTtsService::ArcTtsService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->tts()->AddObserver(this);
}

ArcTtsService::~ArcTtsService() {
  arc_bridge_service()->tts()->RemoveObserver(this);
}

void ArcTtsService::OnInstanceReady() {
  mojom::TtsInstance* tts_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->tts(), Init);
  DCHECK(tts_instance);
  tts_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcTtsService::OnTtsEvent(uint32_t id,
                               mojom::TtsEventType event_type,
                               uint32_t char_index,
                               const std::string& error_msg) {
  if (!TtsController::GetInstance())
    return;

  TtsEventType chrome_event_type;
  switch (event_type) {
    case mojom::TtsEventType::START:
      chrome_event_type = TTS_EVENT_START;
      break;
    case mojom::TtsEventType::END:
      chrome_event_type = TTS_EVENT_END;
      break;
    case mojom::TtsEventType::INTERRUPTED:
      chrome_event_type = TTS_EVENT_INTERRUPTED;
      break;
    case mojom::TtsEventType::ERROR:
      chrome_event_type = TTS_EVENT_ERROR;
      break;
  }
  TtsController::GetInstance()->OnTtsEvent(id, chrome_event_type, char_index,
                                           error_msg);
}

}  // namespace arc
