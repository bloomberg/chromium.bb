// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_ARC_HOME_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_ARC_HOME_SERVICE_H_

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/voice_interaction_arc_home.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {
struct AXSnapshotNodeAndroid;
}  // ui

namespace arc {

// ArcVoiceInteractionArcHomeService provides view hierarchy to to ARC to be
// used by VoiceInteractionSession. This class lives on the UI thread.
class ArcVoiceInteractionArcHomeService
    : public ArcService,
      public mojom::VoiceInteractionArcHomeHost,
      public InstanceHolder<mojom::VoiceInteractionArcHomeInstance>::Observer {
 public:
  explicit ArcVoiceInteractionArcHomeService(ArcBridgeService* bridge_service);
  ~ArcVoiceInteractionArcHomeService() override;

  // InstanceHolder<mojom::VoiceInteractionArcHomeInstance> overrides;
  void OnInstanceReady() override;

  // Gets view hierarchy from current focused app and send it to ARC.
  void GetVoiceInteractionStructure(
      const GetVoiceInteractionStructureCallback& callback) override;
  void OnVoiceInteractionOobeSetupComplete() override;

  static mojom::VoiceInteractionStructurePtr
  CreateVoiceInteractionStructureForTesting(
      const ui::AXSnapshotNodeAndroid& view_structure);

 private:
  mojo::Binding<mojom::VoiceInteractionArcHomeHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcVoiceInteractionArcHomeService);
};

}  // namespace arc
#endif  // CHROME_BROWSER_CHROMEOS_ARC_VOICE_INTERACTION_ARC_VOICE_INTERACTION_ARC_HOME_SERVICE_H_
