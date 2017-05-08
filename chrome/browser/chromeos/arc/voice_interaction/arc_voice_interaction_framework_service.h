// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_FRAMEWORK_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_FRAMEWORK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/voice_interaction_framework.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/accelerators/accelerator.h"

namespace arc {

// This provides voice interaction context (currently screenshots)
// to ARC to be used by VoiceInteractionSession. This class lives on the UI
// thread.
class ArcVoiceInteractionFrameworkService
    : public ArcService,
      public mojom::VoiceInteractionFrameworkHost,
      public ui::AcceleratorTarget,
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

  // mojom::VoiceInteractionFrameworkHost overrides.
  void CaptureFocusedWindow(
      const CaptureFocusedWindowCallback& callback) override;
  void CaptureFullscreen(const CaptureFullscreenCallback& callback) override;
  void OnMetalayerClosed() override;

  bool IsMetalayerSupported();
  void ShowMetalayer(const base::Closure& closed);
  void HideMetalayer();

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

 private:
  void SetMetalayerVisibility(bool visible);

  mojo::Binding<mojom::VoiceInteractionFrameworkHost> binding_;
  base::Closure metalayer_closed_callback_;

  DISALLOW_COPY_AND_ASSIGN(ArcVoiceInteractionFrameworkService);
};

}  // namespace arc
#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_FRAMEWORK_SERVICE_H_
