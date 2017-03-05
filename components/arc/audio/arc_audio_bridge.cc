// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/audio/arc_audio_bridge.h"

#include "ash/common/system/chromeos/audio/tray_audio.h"
#include "base/logging.h"
#include "chromeos/audio/audio_device.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcAudioBridge::ArcAudioBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->audio()->AddObserver(this);
  if (chromeos::CrasAudioHandler::IsInitialized()) {
    cras_audio_handler_ = chromeos::CrasAudioHandler::Get();
    cras_audio_handler_->AddAudioObserver(this);
  }
}

ArcAudioBridge::~ArcAudioBridge() {
  if (cras_audio_handler_ && chromeos::CrasAudioHandler::IsInitialized()) {
    cras_audio_handler_->RemoveAudioObserver(this);
  }
  arc_bridge_service()->audio()->RemoveObserver(this);
}

void ArcAudioBridge::OnInstanceReady() {
  mojom::AudioInstance* audio_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->audio(), Init);
  DCHECK(audio_instance);  // the instance on ARC side is too old.
  audio_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcAudioBridge::ShowVolumeControls() {
  DVLOG(2) << "ArcAudioBridge::ShowVolumeControls";
  ash::TrayAudio::ShowPopUpVolumeView();
}

void ArcAudioBridge::OnAudioNodesChanged() {
  uint64_t output_id = cras_audio_handler_->GetPrimaryActiveOutputNode();
  const chromeos::AudioDevice* output_device =
      cras_audio_handler_->GetDeviceFromId(output_id);
  bool headphone_inserted =
      (output_device &&
       output_device->type == chromeos::AudioDeviceType::AUDIO_TYPE_HEADPHONE);

  uint64_t input_id = cras_audio_handler_->GetPrimaryActiveInputNode();
  const chromeos::AudioDevice* input_device =
      cras_audio_handler_->GetDeviceFromId(input_id);
  bool microphone_inserted =
      (input_device &&
       input_device->type == chromeos::AudioDeviceType::AUDIO_TYPE_MIC);

  DVLOG(1) << "HEADPHONE " << headphone_inserted << " MICROPHONE "
           << microphone_inserted;
  SendSwitchState(headphone_inserted, microphone_inserted);
}

void ArcAudioBridge::OnOutputNodeVolumeChanged(uint64_t node_id, int volume) {
  DVLOG(1) << "Output node " << node_id << " volume " << volume;
  volume_ = volume;
  SendVolumeState();
}

void ArcAudioBridge::OnOutputMuteChanged(bool mute_on, bool system_adjust) {
  DVLOG(1) << "Output mute " << mute_on << " by system " << system_adjust;
  muted_ = mute_on;
  SendVolumeState();
}

void ArcAudioBridge::SendSwitchState(bool headphone_inserted,
                                     bool microphone_inserted) {
  uint32_t switch_state = 0;
  if (headphone_inserted) {
    switch_state |=
        (1 << static_cast<uint32_t>(mojom::AudioSwitch::SW_HEADPHONE_INSERT));
  }
  if (microphone_inserted) {
    switch_state |=
        (1 << static_cast<uint32_t>(mojom::AudioSwitch::SW_MICROPHONE_INSERT));
  }

  DVLOG(1) << "Send switch state " << switch_state;
  mojom::AudioInstance* audio_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->audio(), NotifySwitchState);
  if (audio_instance)
    audio_instance->NotifySwitchState(switch_state);
}

void ArcAudioBridge::SendVolumeState() {
  DVLOG(1) << "Send volume " << volume_ << " muted " << muted_;
  mojom::AudioInstance* audio_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->audio(), NotifyVolumeState);
  if (audio_instance)
    audio_instance->NotifyVolumeState(volume_, muted_);
}

}  // namespace arc
