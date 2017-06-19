// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_FRAMEWORK_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_FRAMEWORK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/voice_interaction_framework.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_handler.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace arc {

// This provides voice interaction context (currently screenshots)
// to ARC to be used by VoiceInteractionSession. This class lives on the UI
// thread.
class ArcVoiceInteractionFrameworkService
    : public ArcService,
      public mojom::VoiceInteractionFrameworkHost,
      public ui::AcceleratorTarget,
      public ui::EventHandler,
      public InstanceHolder<
          mojom::VoiceInteractionFrameworkInstance>::Observer {
 public:
  explicit ArcVoiceInteractionFrameworkService(
      ArcBridgeService* bridge_service);
  ~ArcVoiceInteractionFrameworkService() override;

  // InstanceHolder<mojom::VoiceInteractionFrameworkInstance> overrides.
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // ui::AcceleratorTarget overrides.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // ui::EventHandler overrides.
  void OnTouchEvent(ui::TouchEvent* event) override;

  // mojom::VoiceInteractionFrameworkHost overrides.
  void CaptureFocusedWindow(
      const CaptureFocusedWindowCallback& callback) override;
  void CaptureFullscreen(const CaptureFullscreenCallback& callback) override;
  void OnMetalayerClosed() override;
  void SetMetalayerEnabled(bool enabled) override;

  bool IsMetalayerSupported();
  void ShowMetalayer(const base::Closure& closed);
  void HideMetalayer();

  // Starts a voice interaction session after user-initiated interaction.
  // Records a timestamp and sets number of allowed requests to 2 since by
  // design, there will be one request for screenshot and the other for
  // voice interaction context.
  // |region| refers to the selected region on the screen to be passed to
  // VoiceInteractionFrameworkInstance::StartVoiceInteractionSessionForRegion().
  // If |region| is empty,
  // VoiceInteractionFrameworkInstance::StartVoiceInteraction() is called.
  void StartSessionFromUserInteraction(const gfx::Rect& region);

  // Turn on / off voice interaction in ARC.
  // TODO(muyuanli): We should also check on Chrome side once CrOS side settings
  // are ready (tracked separately at crbug.com/727873).
  void SetVoiceInteractionEnabled(bool enable);

  // Turn on / off voice interaction context (screenshot and structural data)
  // in ARC.
  void SetVoiceInteractionContextEnabled(bool enable);

  // Checks whether the caller is called within the time limit since last user
  // initiated interaction. Logs UMA metric when it's not.
  bool ValidateTimeSinceUserInteraction();

  // Start the voice interaction setup wizard in container.
  void StartVoiceInteractionSetupWizard();

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

 private:
  void SetMetalayerVisibility(bool visible);

  void CallAndResetMetalayerCallback();

  bool InitiateUserInteraction();

  mojo::Binding<mojom::VoiceInteractionFrameworkHost> binding_;
  base::Closure metalayer_closed_callback_;
  bool metalayer_enabled_ = false;

  // The time when a user initated an interaction.
  base::TimeTicks user_interaction_start_time_;

  // The number of allowed requests from container. Maximum is 2 (1 for
  // screenshot and 1 for view hierarchy). This amount decreases after each
  // context request or resets when allowed time frame is elapsed.  When this
  // quota is 0, but we still get requests from the container side, we assume
  // something malicious is going on.
  int32_t context_request_remaining_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ArcVoiceInteractionFrameworkService);
};

}  // namespace arc
#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_FRAMEWORK_SERVICE_H_
