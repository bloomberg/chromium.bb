// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/volume_view.h"

#include "ash/ash_constants.h"
#include "ash/shell.h"
#include "ash/system/audio/tray_audio.h"
#include "ash/system/audio/tray_audio_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_container.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kVolumeImageWidth = 25;
const int kVolumeImageHeight = 25;
const int kSeparatorSize = 3;
const int kSeparatorVerticalInset = 8;
const int kSliderRightPaddingToVolumeViewEdge = 17;
const int kExtraPaddingBetweenBarAndMore = 10;
const int kExtraPaddingBetweenIconAndSlider = 8;
const int kBoxLayoutPadding = 2;

// IDR_AURA_UBER_TRAY_VOLUME_LEVELS contains 5 images,
// The one for mute is at the 0 index and the other
// four are used for ascending volume levels.
const int kVolumeLevels = 4;

}  // namespace

namespace ash {
namespace tray {

class VolumeButton : public views::ToggleImageButton {
 public:
   VolumeButton(views::ButtonListener* listener,
                system::TrayAudioDelegate* audio_delegate)
      : views::ToggleImageButton(listener),
        audio_delegate_(audio_delegate),
        image_index_(-1) {
    SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
    image_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME_LEVELS);
    Update();
  }

  ~VolumeButton() override {}

  void Update() {
    float level =
        static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f;
    int image_index = audio_delegate_->IsOutputAudioMuted() ?
        0 : (level == 1.0 ?
             kVolumeLevels :
             std::max(1, int(std::ceil(level * (kVolumeLevels - 1)))));
    if (image_index != image_index_) {
      gfx::Rect region(0, image_index * kVolumeImageHeight,
                       kVolumeImageWidth, kVolumeImageHeight);
      gfx::ImageSkia image_skia = gfx::ImageSkiaOperations::ExtractSubset(
          *(image_.ToImageSkia()), region);
      SetImage(views::CustomButton::STATE_NORMAL, &image_skia);
      image_index_ = image_index;
    }
  }

 private:
  // views::View:
  gfx::Size GetPreferredSize() const override {
    gfx::Size size = views::ToggleImageButton::GetPreferredSize();
    size.set_height(kTrayPopupItemHeight);
    return size;
  }

  // views::CustomButton:
  void StateChanged() override {
    if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
      set_background(
          views::Background::CreateSolidBackground(kHoverBackgroundColor));
    } else {
      set_background(nullptr);
    }
  }

  system::TrayAudioDelegate* audio_delegate_;
  gfx::Image image_;
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(VolumeButton);
};

VolumeView::VolumeView(SystemTrayItem* owner,
                       system::TrayAudioDelegate* audio_delegate,
                       bool is_default_view)
    : owner_(owner),
      audio_delegate_(audio_delegate),
      icon_(NULL),
      slider_(NULL),
      device_type_(NULL),
      more_(NULL),
      is_default_view_(is_default_view) {
  SetFocusBehavior(FocusBehavior::NEVER);
  views::BoxLayout* box_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kBoxLayoutPadding);
  box_layout->SetDefaultFlex(0);
  SetLayoutManager(box_layout);

  icon_ = new VolumeButton(this, audio_delegate_);
  icon_->SetBorder(views::Border::CreateEmptyBorder(
      0, kTrayPopupPaddingHorizontal, 0, kExtraPaddingBetweenIconAndSlider));
  AddChildView(icon_);

  slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
  slider_->set_focus_border_color(kFocusBorderColor);
  slider_->SetValue(
      static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f);
  slider_->SetAccessibleName(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_ASH_STATUS_TRAY_VOLUME));
  slider_->SetBorder(
      views::Border::CreateEmptyBorder(0, 0, 0, kTrayPopupPaddingBetweenItems));
  AddChildView(slider_);
  box_layout->SetFlexForView(slider_, 1);

  separator_ = new views::Separator(views::Separator::VERTICAL);
  separator_->SetColor(kButtonStrokeColor);
  separator_->SetPreferredSize(kSeparatorSize);
  separator_->SetBorder(views::Border::CreateEmptyBorder(
      kSeparatorVerticalInset, 0, kSeparatorVerticalInset, kBoxLayoutPadding));

  more_region_ = new TrayPopupItemContainer(separator_, true, false);
  more_region_->SetBorder(
      views::Border::CreateEmptyBorder(0, 0, 0, kTrayPopupPaddingBetweenItems));
  AddChildView(more_region_);

  device_type_ = new views::ImageView;
  more_region_->AddChildView(device_type_);

  more_ = new views::ImageView;
  more_->EnableCanvasFlippingForRTLUI(true);
  more_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_UBER_TRAY_MORE).ToImageSkia());
  more_region_->AddChildView(more_);

  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  Update();
}

VolumeView::~VolumeView() {
}

void VolumeView::Update() {
  icon_->Update();
  slider_->UpdateState(!audio_delegate_->IsOutputAudioMuted());
  UpdateDeviceTypeAndMore();
  Layout();
}

void VolumeView::SetVolumeLevel(float percent) {
  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, do not set change
  // slider value if the change is less than 1%.
  if (std::abs(percent-slider_->value()) < 0.01)
    return;
  slider_->SetValue(percent);
  // It is possible that the volume was (un)muted, but the actual volume level
  // did not change. In that case, setting the value of the slider won't
  // trigger an update. So explicitly trigger an update.
  Update();
  slider_->set_enable_accessibility_events(true);
}

void VolumeView::UpdateDeviceTypeAndMore() {
  bool show_more = is_default_view_ && TrayAudio::ShowAudioDeviceMenu() &&
                   audio_delegate_->HasAlternativeSources();
  slider_->SetBorder(views::Border::CreateEmptyBorder(
      0, 0, 0, show_more ? kTrayPopupPaddingBetweenItems
                         : kSliderRightPaddingToVolumeViewEdge));

  if (!show_more) {
    more_region_->SetVisible(false);
    return;
  }

  // Show output device icon if necessary.
  int device_icon = audio_delegate_->GetActiveOutputDeviceIconId();
  if (device_icon != system::TrayAudioDelegate::kNoAudioDeviceIcon) {
    device_type_->SetVisible(true);
    device_type_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            device_icon).ToImageSkia());
    more_region_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
  } else {
    device_type_->SetVisible(false);
    more_region_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0,
        kTrayPopupPaddingBetweenItems + kExtraPaddingBetweenBarAndMore));
  }
  more_region_->SetVisible(true);
}

void VolumeView::HandleVolumeUp(float level) {
  audio_delegate_->SetOutputVolumeLevel(level);
  if (audio_delegate_->IsOutputAudioMuted() &&
      level > audio_delegate_->GetOutputDefaultVolumeMuteLevel()) {
    audio_delegate_->SetOutputAudioIsMuted(false);
  }
}

void VolumeView::HandleVolumeDown(float level) {
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
  CHECK(sender == icon_);
  bool mute_on = !audio_delegate_->IsOutputAudioMuted();
  audio_delegate_->SetOutputAudioIsMuted(mute_on);
  if (!mute_on)
    audio_delegate_->AdjustOutputVolumeToAudibleLevel();
  icon_->Update();
}

void VolumeView::SliderValueChanged(views::Slider* sender,
                                    float value,
                                    float old_value,
                                    views::SliderChangeReason reason) {
  if (reason == views::VALUE_CHANGED_BY_USER) {
    float new_volume = value * 100.0f;
    float current_volume = audio_delegate_->GetOutputVolumeLevel();
    // Do not call change audio volume if the difference is less than
    // 1%, which is beyond cras audio api's granularity for output volume.
    if (std::abs(new_volume - current_volume) < 1.0f)
      return;
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        is_default_view_ ?
        ash::UMA_STATUS_AREA_CHANGED_VOLUME_MENU :
        ash::UMA_STATUS_AREA_CHANGED_VOLUME_POPUP);
    if (new_volume > current_volume)
      HandleVolumeUp(new_volume);
    else
      HandleVolumeDown(new_volume);
  }
  icon_->Update();
}

bool VolumeView::PerformAction(const ui::Event& event) {
  if (!more_region_->visible())
    return false;
  owner_->TransitionDetailedView();
  return true;
}

void VolumeView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Separator's prefered size is based on set bounds. When an empty bounds is
  // set on first layout this causes BoxLayout to ignore the separator. Reset
  // its height on each bounds change so that it is laid out properly.
  separator_->SetSize(gfx::Size(kSeparatorSize, bounds().height()));
}

void VolumeView::GetAccessibleState(ui::AXViewState* state) {
  // Intentionally overrides ActionableView, leaving |state| unset. A slider
  // childview exposes accessibility data.
}

}  // namespace tray
}  // namespace ash
