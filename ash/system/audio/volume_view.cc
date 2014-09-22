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
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kVolumeImageWidth = 25;
const int kVolumeImageHeight = 25;
const int kBarSeparatorWidth = 25;
const int kBarSeparatorHeight = 30;
const int kSliderRightPaddingToVolumeViewEdge = 17;
const int kExtraPaddingBetweenBarAndMore = 10;

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

  virtual ~VolumeButton() {}

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
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    gfx::Size size = views::ToggleImageButton::GetPreferredSize();
    size.set_height(kTrayPopupItemHeight);
    return size;
  }

  system::TrayAudioDelegate* audio_delegate_;
  gfx::Image image_;
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(VolumeButton);
};

class VolumeSlider : public views::Slider {
 public:
  VolumeSlider(views::SliderListener* listener,
               system::TrayAudioDelegate* audio_delegate)
      : views::Slider(listener, views::Slider::HORIZONTAL),
        audio_delegate_(audio_delegate) {
    set_focus_border_color(kFocusBorderColor);
    SetValue(
        static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f);
    SetAccessibleName(
            ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                IDS_ASH_STATUS_TRAY_VOLUME));
    Update();
  }
  virtual ~VolumeSlider() {}

  void Update() {
    UpdateState(!audio_delegate_->IsOutputAudioMuted());
  }

 private:
  system::TrayAudioDelegate* audio_delegate_;

  DISALLOW_COPY_AND_ASSIGN(VolumeSlider);
};

// Vertical bar separator that can be placed on the VolumeView.
class BarSeparator : public views::View {
 public:
  BarSeparator() {}
  virtual ~BarSeparator() {}

  // Overriden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kBarSeparatorWidth, kBarSeparatorHeight);
  }

 private:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(gfx::Rect(width() / 2, 0, 1, height()),
                     kButtonStrokeColor);
  }

  DISALLOW_COPY_AND_ASSIGN(BarSeparator);
};

VolumeView::VolumeView(SystemTrayItem* owner,
                       system::TrayAudioDelegate* audio_delegate,
                       bool is_default_view)
    : owner_(owner),
      audio_delegate_(audio_delegate),
      icon_(NULL),
      slider_(NULL),
      bar_(NULL),
      device_type_(NULL),
      more_(NULL),
      is_default_view_(is_default_view) {
  SetFocusable(false);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

  icon_ = new VolumeButton(this, audio_delegate_);
  AddChildView(icon_);

  slider_ = new VolumeSlider(this, audio_delegate_);
  AddChildView(slider_);

  bar_ = new BarSeparator;
  AddChildView(bar_);

  device_type_ = new views::ImageView;
  AddChildView(device_type_);

  more_ = new views::ImageView;
  more_->EnableCanvasFlippingForRTLUI(true);
  more_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_UBER_TRAY_MORE).ToImageSkia());
  AddChildView(more_);

  Update();
}

VolumeView::~VolumeView() {
}

void VolumeView::Update() {
  icon_->Update();
  slider_->Update();
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
  // The change in volume will be reflected via accessibility system events,
  // so we prevent the UI event from being sent here.
  slider_->set_enable_accessibility_events(false);
  slider_->SetValue(percent);
  // It is possible that the volume was (un)muted, but the actual volume level
  // did not change. In that case, setting the value of the slider won't
  // trigger an update. So explicitly trigger an update.
  Update();
  slider_->set_enable_accessibility_events(true);
}

void VolumeView::UpdateDeviceTypeAndMore() {
  if (!TrayAudio::ShowAudioDeviceMenu() || !is_default_view_) {
    more_->SetVisible(false);
    bar_->SetVisible(false);
    device_type_->SetVisible(false);
    return;
  }

  bool show_more = audio_delegate_->HasAlternativeSources();
  more_->SetVisible(show_more);
  bar_->SetVisible(show_more);

  // Show output device icon if necessary.
  int device_icon = audio_delegate_->GetActiveOutputDeviceIconId();
  if (device_icon != system::TrayAudioDelegate::kNoAudioDeviceIcon) {
    device_type_->SetVisible(true);
    device_type_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            device_icon).ToImageSkia());
  } else {
    device_type_->SetVisible(false);
  }
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

void VolumeView::Layout() {
  views::View::Layout();

  if (!more_->visible()) {
    int w = width() - slider_->bounds().x() -
            kSliderRightPaddingToVolumeViewEdge;
    slider_->SetSize(gfx::Size(w, slider_->height()));
    return;
  }

  // Make sure the chevron always has the full size.
  gfx::Size size = more_->GetPreferredSize();
  gfx::Rect bounds(size);
  bounds.set_x(width() - size.width() - kTrayPopupPaddingBetweenItems);
  bounds.set_y((height() - size.height()) / 2);
  more_->SetBoundsRect(bounds);

  // Layout either bar_ or device_type_ at the left of the more_ button.
  views::View* view_left_to_more;
  if (device_type_->visible())
    view_left_to_more = device_type_;
  else
    view_left_to_more = bar_;
  gfx::Size view_size = view_left_to_more->GetPreferredSize();
  gfx::Rect view_bounds(view_size);
  view_bounds.set_x(more_->bounds().x() - view_size.width() -
                    kExtraPaddingBetweenBarAndMore);
  view_bounds.set_y((height() - view_size.height()) / 2);
  view_left_to_more->SetBoundsRect(view_bounds);

  // Layout vertical bar next to view_left_to_more if device_type_ is visible.
  if (device_type_->visible()) {
    gfx::Size bar_size = bar_->GetPreferredSize();
    gfx::Rect bar_bounds(bar_size);
    bar_bounds.set_x(view_left_to_more->bounds().x() - bar_size.width());
    bar_bounds.set_y((height() - bar_size.height()) / 2);
    bar_->SetBoundsRect(bar_bounds);
  }

  // Layout slider, calculate slider width.
  gfx::Rect slider_bounds = slider_->bounds();
  slider_bounds.set_width(
      bar_->bounds().x()
      - (device_type_->visible() ? 0 : kTrayPopupPaddingBetweenItems)
      - slider_bounds.x());
  slider_->SetBoundsRect(slider_bounds);
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
  if (!more_->visible())
    return false;
  owner_->TransitionDetailedView();
  return true;
}

}  // namespace tray
}  // namespace ash
