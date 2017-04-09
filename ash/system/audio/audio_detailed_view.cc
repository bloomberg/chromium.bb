// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

namespace {

base::string16 GetAudioDeviceName(const chromeos::AudioDevice& device) {
  switch (device.type) {
    case chromeos::AUDIO_TYPE_FRONT_MIC:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_FRONT_MIC);
    case chromeos::AUDIO_TYPE_HEADPHONE:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HEADPHONE);
    case chromeos::AUDIO_TYPE_INTERNAL_SPEAKER:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_SPEAKER);
    case chromeos::AUDIO_TYPE_INTERNAL_MIC:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_MIC);
    case chromeos::AUDIO_TYPE_REAR_MIC:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_REAR_MIC);
    case chromeos::AUDIO_TYPE_USB:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_USB_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case chromeos::AUDIO_TYPE_BLUETOOTH:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_BLUETOOTH_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case chromeos::AUDIO_TYPE_HDMI:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HDMI_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case chromeos::AUDIO_TYPE_MIC:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_MIC_JACK_DEVICE);
    default:
      return base::UTF8ToUTF16(device.display_name);
  }
}

}  // namespace

using chromeos::CrasAudioHandler;

namespace ash {
namespace tray {

AudioDetailedView::AudioDetailedView(SystemTrayItem* owner)
    : TrayDetailsView(owner) {
  CreateItems();
  Update();
}

AudioDetailedView::~AudioDetailedView() {}

void AudioDetailedView::Update() {
  UpdateAudioDevices();
  Layout();
}

void AudioDetailedView::AddInputHeader() {
  AddScrollListInfoItem(IDS_ASH_STATUS_TRAY_AUDIO_INPUT,
                        kSystemMenuAudioInputIcon);
}

void AudioDetailedView::AddOutputHeader() {
  AddScrollListInfoItem(IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT,
                        kSystemMenuAudioOutputIcon);
}

void AudioDetailedView::AddScrollListInfoItem(int text_id,
                                              const gfx::VectorIcon& icon) {
  TriView* header = TrayPopupUtils::CreateDefaultRowView();
  TrayPopupUtils::ConfigureAsStickyHeader(header);
  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(gfx::CreateVectorIcon(
      icon, GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_ProminentButtonColor)));
  header->AddView(TriView::Container::START, image_view);

  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(l10n_util::GetStringUTF16(text_id));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
  style.SetupLabel(label);
  header->AddView(TriView::Container::CENTER, label);

  header->SetContainerVisible(TriView::Container::END, false);
  scroll_content()->AddChildView(header);
}

HoverHighlightView* AudioDetailedView::AddScrollListItem(
    const base::string16& text,
    bool highlight,
    bool checked) {
  HoverHighlightView* container = new HoverHighlightView(this);

  container->AddLabelRow(text);
  TrayPopupUtils::InitializeAsCheckableRow(container, checked);

  scroll_content()->AddChildView(container);
  return container;
}

void AudioDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_AUDIO);
}

void AudioDetailedView::UpdateAudioDevices() {
  output_devices_.clear();
  input_devices_.clear();
  chromeos::AudioDeviceList devices;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->GetAudioDevices(&devices);
  bool has_dual_internal_mic = audio_handler->HasDualInternalMic();
  bool is_front_or_rear_mic_active = false;
  for (size_t i = 0; i < devices.size(); ++i) {
    // Don't display keyboard mic or aokr type.
    if (!devices[i].is_for_simple_usage())
      continue;
    if (devices[i].is_input) {
      // Do not expose the internal front and rear mic to UI.
      if (has_dual_internal_mic &&
          audio_handler->IsFrontOrRearMic(devices[i])) {
        if (devices[i].active)
          is_front_or_rear_mic_active = true;
        continue;
      }
      input_devices_.push_back(devices[i]);
    } else {
      output_devices_.push_back(devices[i]);
    }
  }

  // Expose the dual internal mics as one device (internal mic) to user.
  if (has_dual_internal_mic) {
    // Create stub internal mic entry for UI rendering, which representing
    // both internal front and rear mics.
    chromeos::AudioDevice internal_mic;
    internal_mic.is_input = true;
    internal_mic.stable_device_id_version = 2;
    internal_mic.type = chromeos::AUDIO_TYPE_INTERNAL_MIC;
    internal_mic.active = is_front_or_rear_mic_active;
    input_devices_.push_back(internal_mic);
  }

  UpdateScrollableList();
}

void AudioDetailedView::UpdateScrollableList() {
  scroll_content()->RemoveAllChildViews(true);
  device_map_.clear();

  // Add audio output devices.
  const bool has_output_devices = output_devices_.size() > 0;
  if (has_output_devices)
    AddOutputHeader();

  for (size_t i = 0; i < output_devices_.size(); ++i) {
    HoverHighlightView* container = AddScrollListItem(
        GetAudioDeviceName(output_devices_[i]), false /* highlight */,
        output_devices_[i].active); /* checkmark if active */
    device_map_[container] = output_devices_[i];
  }

  if (has_output_devices) {
    scroll_content()->AddChildView(
        TrayPopupUtils::CreateListSubHeaderSeparator());
  }

  // Add audio input devices.
  const bool has_input_devices = input_devices_.size() > 0;
  if (has_input_devices)
    AddInputHeader();

  for (size_t i = 0; i < input_devices_.size(); ++i) {
    HoverHighlightView* container = AddScrollListItem(
        GetAudioDeviceName(input_devices_[i]), false /* highlight */,
        input_devices_[i].active); /* checkmark if active */
    device_map_[container] = input_devices_[i];
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

void AudioDetailedView::HandleViewClicked(views::View* view) {
  AudioDeviceMap::iterator iter = device_map_.find(view);
  if (iter == device_map_.end())
    return;
  chromeos::AudioDevice device = iter->second;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (device.type == chromeos::AUDIO_TYPE_INTERNAL_MIC &&
      audio_handler->HasDualInternalMic()) {
    audio_handler->SwitchToFrontOrRearMic();
  } else {
    audio_handler->SwitchToDevice(device, true,
                                  CrasAudioHandler::ACTIVATE_BY_USER);
  }
}

}  // namespace tray
}  // namespace ash
