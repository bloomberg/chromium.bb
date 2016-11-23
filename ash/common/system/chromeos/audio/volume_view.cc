// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/audio/volume_view.h"

#include <algorithm>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/chromeos/audio/tray_audio_delegate.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_container.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/slider.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {
const int kVolumeImageWidth = 25;
const int kVolumeImageHeight = 25;
const int kSeparatorSize = 3;
const int kSeparatorVerticalInset = 8;
const int kBoxLayoutPadding = 2;

// IDR_AURA_UBER_TRAY_VOLUME_LEVELS contains 5 images,
// The one for mute is at the 0 index and the other
// four are used for ascending volume levels.
const int kVolumeLevels = 4;

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
    SetFocusBehavior(FocusBehavior::ALWAYS);
    AddChildView(image_);
    if (MaterialDesignController::IsSystemTrayMenuMaterial())
      SetInkDropMode(InkDropMode::ON);
    Update();

    set_notify_enter_exit_on_child(true);
  }

  ~VolumeButton() override {}

  void Update() {
    float level =
        static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f;
    int volume_levels = MaterialDesignController::IsSystemTrayMenuMaterial()
                            ? arraysize(kVolumeLevelIcons) - 1
                            : kVolumeLevels;
    int image_index =
        audio_delegate_->IsOutputAudioMuted()
            ? 0
            : (level == 1.0 ? volume_levels
                            : std::max(1, static_cast<int>(std::ceil(
                                              level * (volume_levels - 1)))));
    if (image_index != image_index_) {
      gfx::ImageSkia image_skia;
      if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
        image_skia = gfx::CreateVectorIcon(*kVolumeLevelIcons[image_index],
                                           kMenuIconColor);
      } else {
        gfx::Rect region(0, image_index * kVolumeImageHeight, kVolumeImageWidth,
                         kVolumeImageHeight);
        gfx::Image image =
            ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                IDR_AURA_UBER_TRAY_VOLUME_LEVELS);
        image_skia = gfx::ImageSkiaOperations::ExtractSubset(
            *(image.ToImageSkia()), region);
      }
      image_->SetImage(&image_skia);
      image_index_ = image_index;
    }
  }

 private:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    node_data->SetName(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_VOLUME_MUTE));
    node_data->role = ui::AX_ROLE_TOGGLE_BUTTON;
    if (audio_delegate_->IsOutputAudioMuted())
      node_data->AddStateFlag(ui::AX_STATE_PRESSED);
  }

  // views::CustomButton:
  void StateChanged() override {
    if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
      if (!MaterialDesignController::IsSystemTrayMenuMaterial()) {
        set_background(
            views::Background::CreateSolidBackground(kHoverBackgroundColor));
      }
    } else {
      set_background(nullptr);
    }
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
      separator_(nullptr),
      device_type_(nullptr),
      is_default_view_(is_default_view) {
  SetFocusBehavior(FocusBehavior::NEVER);
  SetLayoutManager(new views::FillLayout);
  AddChildView(tri_view_);

  icon_ = new VolumeButton(owner, this, audio_delegate_);
  tri_view_->AddView(TriView::Container::START, icon_);

  slider_ = TrayPopupUtils::CreateSlider(this);
  slider_->SetValue(
      static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f);
  slider_->SetAccessibleName(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_ASH_STATUS_TRAY_VOLUME));
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
  more_button_->SetFocusBehavior(FocusBehavior::NEVER);

  device_type_ = TrayPopupUtils::CreateMoreImageView();
  more_button_->SetBorder(views::CreateEmptyBorder(
      0, GetTrayConstant(TRAY_POPUP_ITEM_MORE_REGION_HORIZONTAL_INSET), 0,
      GetTrayConstant(TRAY_POPUP_ITEM_MORE_REGION_HORIZONTAL_INSET)));
  more_button_->AddChildView(device_type_);

  views::ImageView* more_arrow = TrayPopupUtils::CreateMoreImageView();
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    more_arrow->SetImage(
        gfx::CreateVectorIcon(kSystemMenuArrowRightIcon, kMenuIconColor));
  } else {
    more_arrow->SetImage(ui::ResourceBundle::GetSharedInstance()
                             .GetImageNamed(IDR_AURA_UBER_TRAY_MORE)
                             .ToImageSkia());
  }
  more_button_->AddChildView(more_arrow);

  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    more_button_->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
    tri_view_->AddView(TriView::Container::END, more_button_);
  } else {
    separator_ = new views::Separator(views::Separator::VERTICAL);
    separator_->SetColor(kButtonStrokeColor);
    separator_->SetPreferredSize(kSeparatorSize);
    separator_->SetBorder(views::CreateEmptyBorder(kSeparatorVerticalInset, 0,
                                                   kSeparatorVerticalInset,
                                                   kBoxLayoutPadding));

    TrayPopupItemContainer* more_container =
        new TrayPopupItemContainer(separator_, true);
    more_container->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
    more_container->AddChildView(more_button_);
    tri_view_->AddView(TriView::Container::END, more_container);
  }

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

  // Show output device icon if necessary.
  device_type_->SetVisible(false);
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    const gfx::VectorIcon& device_icon =
        audio_delegate_->GetActiveOutputDeviceVectorIcon();
    if (!device_icon.is_empty()) {
      device_type_->SetImage(
          gfx::CreateVectorIcon(device_icon, kMenuIconColor));
      device_type_->SetVisible(true);
    }
  } else {
    int device_icon = audio_delegate_->GetActiveOutputDeviceIconId();
    if (device_icon != system::TrayAudioDelegate::kNoAudioDeviceIcon) {
      device_type_->SetImage(ui::ResourceBundle::GetSharedInstance()
                                 .GetImageNamed(device_icon)
                                 .ToImageSkia());
      device_type_->SetVisible(true);
    }
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

bool VolumeView::OnKeyPressed(const ui::KeyEvent& event) {
  const views::FocusManager* focus_manager = GetFocusManager();
  if (enabled() && is_default_view_ && event.key_code() == ui::VKEY_RETURN &&
      focus_manager && focus_manager->GetFocusedView() == slider_) {
    owner_->TransitionDetailedView();
    return true;
  }
  return View::OnKeyPressed(event);
}

void VolumeView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Separator's prefered size is based on set bounds. When an empty bounds is
  // set on first layout this causes BoxLayout to ignore the separator. Reset
  // its height on each bounds change so that it is laid out properly.
  if (separator_)
    separator_->SetSize(gfx::Size(kSeparatorSize, bounds().height()));
}

}  // namespace tray
}  // namespace ash
