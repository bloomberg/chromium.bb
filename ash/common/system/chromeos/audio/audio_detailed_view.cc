// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/audio/audio_detailed_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
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
  const base::string16 text = l10n_util::GetStringUTF16(text_id);
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    TriView* header = TrayPopupUtils::CreateDefaultRowView();
    TrayPopupUtils::ConfigureAsStickyHeader(header);
    views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
    image_view->SetImage(gfx::CreateVectorIcon(
        icon, GetNativeTheme()->GetSystemColor(
                  ui::NativeTheme::kColorId_ProminentButtonColor)));
    header->AddView(TriView::Container::START, image_view);

    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetText(text);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
    style.SetupLabel(label);
    header->AddView(TriView::Container::CENTER, label);

    header->SetContainerVisible(TriView::Container::END, false);
    scroll_content()->AddChildView(header);
  } else {
    views::Label* label = new views::Label(
        text, ui::ResourceBundle::GetSharedInstance().GetFontList(
                  ui::ResourceBundle::BoldFont));

    //  Align info item with checkbox items
    int margin =
        kTrayPopupPaddingHorizontal + kTrayPopupDetailsLabelExtraLeftMargin;
    int left_margin = 0;
    int right_margin = 0;
    if (base::i18n::IsRTL())
      right_margin = margin;
    else
      left_margin = margin;

    label->SetBorder(views::CreateEmptyBorder(
        ash::kTrayPopupPaddingBetweenItems, left_margin,
        ash::kTrayPopupPaddingBetweenItems, right_margin));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetEnabledColor(SkColorSetARGB(192, 0, 0, 0));
    scroll_content()->AddChildView(label);
  }
}

HoverHighlightView* AudioDetailedView::AddScrollListItem(
    const base::string16& text,
    bool highlight,
    bool checked) {
  HoverHighlightView* container = new HoverHighlightView(this);

  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    container->AddLabelRowMd(text);
    if (checked) {
      gfx::ImageSkia check_mark = gfx::CreateVectorIcon(
          gfx::VectorIconId::CHECK_CIRCLE, gfx::kGoogleGreen700);
      container->AddRightIcon(check_mark, check_mark.width());
      container->SetRightViewVisible(true);
      container->SetAccessiblityState(
          HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX);
    } else {
      container->SetAccessiblityState(
          HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
    }
  } else {
    container->AddCheckableLabel(text, highlight, checked);
  }

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
  CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    // Don't display keyboard mic or aokr type.
    if (!devices[i].is_for_simple_usage())
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
  const bool use_md = MaterialDesignController::IsSystemTrayMenuMaterial();
  const bool has_output_devices = output_devices_.size() > 0;
  if (!use_md || has_output_devices)
    AddOutputHeader();

  for (size_t i = 0; i < output_devices_.size(); ++i) {
    HoverHighlightView* container = AddScrollListItem(
        GetAudioDeviceName(output_devices_[i]), false /* highlight */,
        output_devices_[i].active); /* checkmark if active */
    device_map_[container] = output_devices_[i];
  }

  if (!use_md) {
    AddScrollSeparator();
  } else if (has_output_devices) {
    scroll_content()->AddChildView(
        TrayPopupUtils::CreateListSubHeaderSeparator());
  }

  // Add audio input devices.
  const bool has_input_devices = input_devices_.size() > 0;
  if (!use_md || has_input_devices)
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
  CrasAudioHandler::Get()->SwitchToDevice(device, true,
                                          CrasAudioHandler::ACTIVATE_BY_USER);
}

}  // namespace tray
}  // namespace ash
