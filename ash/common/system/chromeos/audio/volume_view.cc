// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/audio/volume_view.h"

#include <algorithm>

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/chromeos/audio/tray_audio_delegate.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/fill_layout.h"

namespace {

const gfx::VectorIcon* const kVolumeLevelIcons[] = {
    &ash::kSystemMenuVolumeMuteIcon,    // Muted.
    &ash::kSystemMenuVolumeLowIcon,     // Low volume.
    &ash::kSystemMenuVolumeMediumIcon,  // Medium volume.
    &ash::kSystemMenuVolumeHighIcon,    // High volume.
    &ash::kSystemMenuVolumeHighIcon,    // Full volume.
};

}  // namespace

namespace ash {
namespace tray {

class VolumeButton : public ButtonListenerActionableView {
 public:
  VolumeButton(SystemTrayItem* owner,
               views::ButtonListener* listener,
               system::TrayAudioDelegate* audio_delegate)
      : ButtonListenerActionableView(owner,
                                     TrayPopupInkDropStyle::HOST_CENTERED,
                                     listener),
        audio_delegate_(audio_delegate),
        image_(TrayPopupUtils::CreateMainImageView()),
        image_index_(-1) {
    TrayPopupUtils::ConfigureContainer(TriView::Container::START, this);
    AddChildView(image_);
    SetInkDropMode(InkDropMode::ON);
    Update();

    set_notify_enter_exit_on_child(true);
  }

  ~VolumeButton() override {}

  void Update() {
    float level =
        static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f;
    int volume_levels = arraysize(kVolumeLevelIcons) - 1;
    int image_index =
        audio_delegate_->IsOutputAudioMuted()
            ? 0
            : (level == 1.0 ? volume_levels
                            : std::max(1, static_cast<int>(std::ceil(
                                              level * (volume_levels - 1)))));
    gfx::ImageSkia image_skia =
        gfx::CreateVectorIcon(*kVolumeLevelIcons[image_index], kMenuIconColor);
    image_->SetImage(&image_skia);
    image_index_ = image_index;
  }

 private:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VOLUME_MUTE));
    node_data->role = ui::AX_ROLE_TOGGLE_BUTTON;
    if (audio_delegate_->IsOutputAudioMuted())
      node_data->AddStateFlag(ui::AX_STATE_PRESSED);
  }

  system::TrayAudioDelegate* audio_delegate_;
  views::ImageView* image_;
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(VolumeButton);
};

VolumeView::VolumeView(SystemTrayItem* owner,
                       system::TrayAudioDelegate* audio_delegate,
                       bool is_default_view)
    : owner_(owner),
      tri_view_(TrayPopupUtils::CreateMultiTargetRowView()),
      audio_delegate_(audio_delegate),
      more_button_(nullptr),
      icon_(nullptr),
      slider_(nullptr),
      device_type_(nullptr),
      is_default_view_(is_default_view) {
  SetLayoutManager(new views::FillLayout);
  AddChildView(tri_view_);

  icon_ = new VolumeButton(owner, this, audio_delegate_);
  tri_view_->AddView(TriView::Container::START, icon_);

  slider_ = TrayPopupUtils::CreateSlider(this);
  slider_->SetValue(
      static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f);
  slider_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VOLUME));
  tri_view_->AddView(TriView::Container::CENTER, slider_);

  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  if (!is_default_view_) {
    tri_view_->SetContainerVisible(TriView::Container::END, false);
    Update();
    return;
  }

  more_button_ = new ButtonListenerActionableView(
      owner_, TrayPopupInkDropStyle::INSET_BOUNDS, this);
  TrayPopupUtils::ConfigureContainer(TriView::Container::END, more_button_);

  more_button_->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  more_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      0, kTrayPopupButtonEndMargin)));
  tri_view_->AddView(TriView::Container::END, more_button_);

  device_type_ = TrayPopupUtils::CreateMoreImageView();
  device_type_->SetVisible(false);
  more_button_->AddChildView(device_type_);

  more_button_->AddChildView(TrayPopupUtils::CreateMoreImageView());
  more_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO));
  Update();
}

VolumeView::~VolumeView() {}

void VolumeView::Update() {
  icon_->Update();
  slider_->UpdateState(!audio_delegate_->IsOutputAudioMuted());
  UpdateDeviceTypeAndMore();
  Layout();
}

void VolumeView::SetVolumeLevel(float percent) {
  // Update volume level to the current audio level.
  Update();

  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, do not set change
  // slider value if the change is less than 1%.
  if (std::abs(percent - slider_->value()) < 0.01)
    return;
  slider_->SetValue(percent);
  // It is possible that the volume was (un)muted, but the actual volume level
  // did not change. In that case, setting the value of the slider won't
  // trigger an update. So explicitly trigger an update.
  Update();
  slider_->set_enable_accessibility_events(true);
}

void VolumeView::UpdateDeviceTypeAndMore() {
  bool show_more = is_default_view_ && audio_delegate_->HasAlternativeSources();

  if (!show_more)
    return;

  const gfx::VectorIcon& device_icon =
      audio_delegate_->GetActiveOutputDeviceVectorIcon();
  const bool target_visibility = !device_icon.is_empty();
  if (target_visibility)
    device_type_->SetImage(gfx::CreateVectorIcon(device_icon, kMenuIconColor));
  if (device_type_->visible() != target_visibility) {
    device_type_->SetVisible(target_visibility);
    device_type_->InvalidateLayout();
  }
}

void VolumeView::HandleVolumeUp(int level) {
  audio_delegate_->SetOutputVolumeLevel(level);
  if (audio_delegate_->IsOutputAudioMuted() &&
      level > audio_delegate_->GetOutputDefaultVolumeMuteLevel()) {
    audio_delegate_->SetOutputAudioIsMuted(false);
  }
}

void VolumeView::HandleVolumeDown(int level) {
  audio_delegate_->SetOutputVolumeLevel(level);
  if (!audio_delegate_->IsOutputAudioMuted() &&
      level <= audio_delegate_->GetOutputDefaultVolumeMuteLevel()) {
    audio_delegate_->SetOutputAudioIsMuted(true);
  } else if (audio_delegate_->IsOutputAudioMuted() &&
             level > audio_delegate_->GetOutputDefaultVolumeMuteLevel()) {
    audio_delegate_->SetOutputAudioIsMuted(false);
  }
}

void VolumeView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == icon_) {
    bool mute_on = !audio_delegate_->IsOutputAudioMuted();
    audio_delegate_->SetOutputAudioIsMuted(mute_on);
    if (!mute_on)
      audio_delegate_->AdjustOutputVolumeToAudibleLevel();
    icon_->Update();
  } else if (sender == more_button_) {
    owner_->TransitionDetailedView();
  } else {
    NOTREACHED() << "Unexpected sender=" << sender->GetClassName() << ".";
  }
}

void VolumeView::SliderValueChanged(views::Slider* sender,
                                    float value,
                                    float old_value,
                                    views::SliderChangeReason reason) {
  if (reason == views::VALUE_CHANGED_BY_USER) {
    int new_volume = static_cast<int>(value * 100);
    int current_volume = audio_delegate_->GetOutputVolumeLevel();
    if (new_volume == current_volume)
      return;
    WmShell::Get()->RecordUserMetricsAction(
        is_default_view_ ? UMA_STATUS_AREA_CHANGED_VOLUME_MENU
                         : UMA_STATUS_AREA_CHANGED_VOLUME_POPUP);
    if (new_volume > current_volume)
      HandleVolumeUp(new_volume);
    else
      HandleVolumeDown(new_volume);
  }
  icon_->Update();
}

}  // namespace tray
}  // namespace ash
