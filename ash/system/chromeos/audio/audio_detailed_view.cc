// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/audio/audio_detailed_view.h"

#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace {

base::string16 GetAudioDeviceName(const chromeos::AudioDevice& device) {
  switch (device.type) {
    case chromeos::AUDIO_TYPE_HEADPHONE:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HEADPHONE);
    case chromeos::AUDIO_TYPE_INTERNAL_SPEAKER:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_SPEAKER);
    case chromeos::AUDIO_TYPE_INTERNAL_MIC:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_MIC);
    case chromeos::AUDIO_TYPE_USB:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_USB_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case chromeos::AUDIO_TYPE_BLUETOOTH:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_BLUETOOTH_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case chromeos::AUDIO_TYPE_HDMI:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_HDMI_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    default:
      return base::UTF8ToUTF16(device.display_name);
  }
}

}  // namespace

using chromeos::CrasAudioHandler;

namespace ash {
namespace tray {

AudioDetailedView::AudioDetailedView(SystemTrayItem* owner,
                                     user::LoginStatus login)
    : TrayDetailsView(owner),
      login_(login) {
  CreateItems();
  Update();
}

AudioDetailedView::~AudioDetailedView() {
}

void AudioDetailedView::Update() {
  UpdateAudioDevices();
  Layout();
}

void AudioDetailedView::AddScrollListInfoItem(const base::string16& text) {
  views::Label* label = new views::Label(
      text,
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BoldFont));

  //  Align info item with checkbox items
  int margin = kTrayPopupPaddingHorizontal +
      kTrayPopupDetailsLabelExtraLeftMargin;
  int left_margin = 0;
  int right_margin = 0;
  if (base::i18n::IsRTL())
    right_margin = margin;
  else
    left_margin = margin;

  label->SetBorder(views::Border::CreateEmptyBorder(
      ash::kTrayPopupPaddingBetweenItems,
      left_margin,
      ash::kTrayPopupPaddingBetweenItems,
      right_margin));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SkColorSetARGB(192, 0, 0, 0));

  scroll_content()->AddChildView(label);
}

HoverHighlightView* AudioDetailedView::AddScrollListItem(
    const base::string16& text,
    gfx::Font::FontStyle style,
    bool checked) {
  HoverHighlightView* container = new HoverHighlightView(this);
  container->AddCheckableLabel(text, style, checked);
  scroll_content()->AddChildView(container);
  return container;
}

void AudioDetailedView::CreateHeaderEntry() {
  CreateSpecialRow(IDS_ASH_STATUS_TRAY_AUDIO, this);
}

void AudioDetailedView::CreateItems() {
  CreateScrollableList();
  CreateHeaderEntry();
}

void AudioDetailedView::UpdateAudioDevices() {
  output_devices_.clear();
  input_devices_.clear();
  chromeos::AudioDeviceList devices;
  CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    // Don't display keyboard mic.
    if (devices[i].type == chromeos::AUDIO_TYPE_KEYBOARD_MIC)
      continue;
    if (devices[i].is_input)
      input_devices_.push_back(devices[i]);
    else
      output_devices_.push_back(devices[i]);
  }
  UpdateScrollableList();
}

void AudioDetailedView::UpdateScrollableList() {
  scroll_content()->RemoveAllChildViews(true);
  device_map_.clear();

  // Add audio output devices.
  AddScrollListInfoItem(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT));
  for (size_t i = 0; i < output_devices_.size(); ++i) {
    HoverHighlightView* container = AddScrollListItem(
        GetAudioDeviceName(output_devices_[i]),
        gfx::Font::NORMAL,
        output_devices_[i].active);  /* checkmark if active */
    device_map_[container] = output_devices_[i];
  }

  AddScrollSeparator();

  // Add audio input devices.
  AddScrollListInfoItem(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INPUT));
  for (size_t i = 0; i < input_devices_.size(); ++i) {
    HoverHighlightView* container = AddScrollListItem(
        GetAudioDeviceName(input_devices_[i]),
        gfx::Font::NORMAL,
        input_devices_[i].active);  /* checkmark if active */
    device_map_[container] = input_devices_[i];
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

void AudioDetailedView::OnViewClicked(views::View* sender) {
  if (sender == footer()->content()) {
    TransitionToDefaultView();
  } else {
    AudioDeviceMap::iterator iter = device_map_.find(sender);
    if (iter == device_map_.end())
      return;
    chromeos::AudioDevice& device = iter->second;
    CrasAudioHandler::Get()->SwitchToDevice(device);
  }
}

}  // namespace tray
}  // namespace ash
