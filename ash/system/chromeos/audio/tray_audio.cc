// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/audio/tray_audio.h"

#include <cmath>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/volume_control_delegate.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

namespace {
const int kVolumeImageWidth = 25;
const int kVolumeImageHeight = 25;
const int kBarSeparatorWidth = 25;
const int kBarSeparatorHeight = 30;
const int kSliderRightPaddingToVolumeViewEdge = 17;
const int kExtraPaddingBetweenBarAndMore = 10;

const int kNoAudioDeviceIcon = -1;

// IDR_AURA_UBER_TRAY_VOLUME_LEVELS contains 5 images,
// The one for mute is at the 0 index and the other
// four are used for ascending volume levels.
const int kVolumeLevels = 4;

bool UseNewAudioHandler() {
  return CommandLine::ForCurrentProcess()->
      HasSwitch(ash::switches::kAshEnableNewAudioHandler);
}

bool IsAudioMuted() {
  if(UseNewAudioHandler()) {
    return chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  } else {
    return Shell::GetInstance()->system_tray_delegate()->
        GetVolumeControlDelegate()->IsAudioMuted();
  }
}

float GetVolumeLevel() {
  if (UseNewAudioHandler()) {
    return chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent() / 100.0f;
  } else {
    return Shell::GetInstance()->system_tray_delegate()->
        GetVolumeControlDelegate()->GetVolumeLevel();
  }
}

int GetAudioDeviceIconId(chromeos::AudioDeviceType type) {
  if (type == chromeos::AUDIO_TYPE_HEADPHONE)
    return IDR_AURA_UBER_TRAY_AUDIO_HEADPHONE;
  else if (type == chromeos::AUDIO_TYPE_USB)
    return IDR_AURA_UBER_TRAY_AUDIO_USB;
  else if (type == chromeos::AUDIO_TYPE_BLUETOOTH)
    return IDR_AURA_UBER_TRAY_AUDIO_BLUETOOTH;
  else if (type == chromeos::AUDIO_TYPE_HDMI)
    return IDR_AURA_UBER_TRAY_AUDIO_HDMI;
  else
    return kNoAudioDeviceIcon;
}

}  // namespace

namespace tray {

class VolumeButton : public views::ToggleImageButton {
 public:
  explicit VolumeButton(views::ButtonListener* listener)
      : views::ToggleImageButton(listener),
        image_index_(-1) {
    SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
    image_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME_LEVELS);
    SetPreferredSize(gfx::Size(kTrayPopupItemHeight, kTrayPopupItemHeight));
    Update();
  }

  virtual ~VolumeButton() {}

  void Update() {
    float level = GetVolumeLevel();
    int image_index = IsAudioMuted() ?
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
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::ToggleImageButton::GetPreferredSize();
    size.set_height(kTrayPopupItemHeight);
    return size;
  }

  gfx::Image image_;
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(VolumeButton);
};

class VolumeSlider : public views::Slider {
 public:
  explicit VolumeSlider(views::SliderListener* listener)
      : views::Slider(listener, views::Slider::HORIZONTAL) {
    set_focus_border_color(kFocusBorderColor);
    SetValue(GetVolumeLevel());
    SetAccessibleName(
            ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                IDS_ASH_STATUS_TRAY_VOLUME));
    Update();
  }
  virtual ~VolumeSlider() {}

  void Update() {
    UpdateState(!IsAudioMuted());
  }

  DISALLOW_COPY_AND_ASSIGN(VolumeSlider);
};

// Vertical bar separator that can be placed on the VolumeView.
class BarSeparator : public views::View {
 public:
  BarSeparator() {}
  virtual ~BarSeparator() {}

 private:
  // Overriden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(kBarSeparatorWidth, kBarSeparatorHeight);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(gfx::Rect(width() / 2, 0, 1, height()),
                     kButtonStrokeColor);
  }

  DISALLOW_COPY_AND_ASSIGN(BarSeparator);
};

class VolumeView : public ActionableView,
                   public views::ButtonListener,
                   public views::SliderListener {
 public:
  VolumeView(SystemTrayItem* owner, bool is_default_view)
      : owner_(owner),
        icon_(NULL),
        slider_(NULL),
        bar_(NULL),
        device_type_(NULL),
        more_(NULL),
        is_default_view_(is_default_view) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

    icon_ = new VolumeButton(this);
    AddChildView(icon_);

    slider_ = new VolumeSlider(this);
    AddChildView(slider_);

    device_type_ = new views::ImageView;
    AddChildView(device_type_);

    bar_ = new BarSeparator;
    AddChildView(bar_);

    more_ = new views::ImageView;
    more_->EnableCanvasFlippingForRTLUI(true);
    more_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_MORE).ToImageSkia());
    AddChildView(more_);

    Update();
  }

  virtual ~VolumeView() {}

  void Update() {
    icon_->Update();
    slider_->Update();
    UpdateDeviceTypeAndMore();
    Layout();
  }

  void SetVolumeLevel(float percent) {
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

 private:
  // Updates bar_, device_type_ icon, and more_ buttons.
  void UpdateDeviceTypeAndMore() {
    if (!UseNewAudioHandler() || !is_default_view_) {
      more_->SetVisible(false);
      bar_->SetVisible(false);
      device_type_->SetVisible(false);
      return;
    }

    chromeos::CrasAudioHandler* audio_handler =
        chromeos::CrasAudioHandler::Get();
    bool show_more = audio_handler->has_alternative_output() ||
                     audio_handler->has_alternative_input();
    more_->SetVisible(show_more);

    // Show output device icon if necessary.
    chromeos::AudioDevice device;
    audio_handler->GetActiveOutputDevice(&device);
    int device_icon = GetAudioDeviceIconId(device.type);
    if (device_icon != kNoAudioDeviceIcon) {
      device_type_->SetVisible(true);
      device_type_->SetImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              device_icon).ToImageSkia());
      bar_->SetVisible(false);
    } else {
      device_type_->SetVisible(false);
      bar_->SetVisible(show_more);
    }
  }

  // Overridden from views::View.
  virtual void Layout() OVERRIDE {
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

    // Layout bar_ or device_type_ at the left of the more_ button.
    views::View* view_left_to_more;
    if (bar_->visible())
      view_left_to_more = bar_;
    else
      view_left_to_more = device_type_;
    gfx::Size bar_size = view_left_to_more->GetPreferredSize();
    gfx::Rect bar_bounds(bar_size);
    bar_bounds.set_x(more_->bounds().x() - bar_size.width() -
                     kExtraPaddingBetweenBarAndMore);
    bar_bounds.set_y((height() - bar_size.height()) / 2);
    view_left_to_more->SetBoundsRect(bar_bounds);


    // Layout slider, calculate slider width.
    gfx::Rect slider_bounds = slider_->bounds();
    slider_bounds.set_width(
        view_left_to_more->bounds().x() - kTrayPopupPaddingBetweenItems
        - slider_bounds.x());
    slider_->SetBoundsRect(slider_bounds);
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    CHECK(sender == icon_);
    if (UseNewAudioHandler()) {
      chromeos::CrasAudioHandler::Get()->SetOutputMute(!IsAudioMuted());
    } else {
      ash::Shell::GetInstance()->system_tray_delegate()->
          GetVolumeControlDelegate()->SetAudioMuted(!IsAudioMuted());
    }
  }

  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE {
    if (reason == views::VALUE_CHANGED_BY_USER) {
      if (UseNewAudioHandler()) {
        chromeos::CrasAudioHandler::Get()->
            SetOutputVolumePercent(value * 100.0f);
      }
      else {
        ash::Shell::GetInstance()->system_tray_delegate()->
            GetVolumeControlDelegate()->SetVolumeLevel(value);
      }
    }
    icon_->Update();
  }

  // Overriden from ActinableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    if (!more_->visible())
      return false;
    owner_->TransitionDetailedView();
    return true;
  }

  SystemTrayItem* owner_;
  VolumeButton* icon_;
  VolumeSlider* slider_;
  BarSeparator* bar_;
  views::ImageView* device_type_;
  views::ImageView* more_;
  bool is_default_view_;

  DISALLOW_COPY_AND_ASSIGN(VolumeView);
};

class AudioDetailedView : public TrayDetailsView,
                          public ViewClickListener {
 public:
  AudioDetailedView(SystemTrayItem* owner, user::LoginStatus login)
      : TrayDetailsView(owner),
        login_(login) {
    CreateItems();
    Update();
  }

  virtual ~AudioDetailedView() {
  }

  void Update() {
    UpdateAudioDevices();
    Layout();
  }

 private:
  void CreateItems() {
    CreateScrollableList();
    CreateHeaderEntry();
  }

  void CreateHeaderEntry() {
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_AUDIO, this);
  }

  void UpdateAudioDevices() {
    output_devices_.clear();
    input_devices_.clear();
    chromeos::AudioDeviceList devices;
    chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].is_input)
        input_devices_.push_back(devices[i]);
      else
        output_devices_.push_back(devices[i]);
    }
    UpdateScrollableList();
  }

  void UpdateScrollableList() {
    scroll_content()->RemoveAllChildViews(true);
    device_map_.clear();

    // Add audio output devices.
    AddScrollListItem(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT),
        gfx::Font::BOLD,
        false);  /* no checkmark */
    for (size_t i = 0; i < output_devices_.size(); ++i) {
      HoverHighlightView* container = AddScrollListItem(
          output_devices_[i].display_name,
          gfx::Font::NORMAL,
          output_devices_[i].active);  /* checkmark if active */
      device_map_[container] = output_devices_[i];
    }

    AddScrollSeparator();

    // Add audio input devices.
    AddScrollListItem(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INPUT),
        gfx::Font::BOLD,
        false);  /* no checkmark */
    for (size_t i = 0; i < input_devices_.size(); ++i) {
      HoverHighlightView* container = AddScrollListItem(
          input_devices_[i].display_name,
          gfx::Font::NORMAL,
          input_devices_[i].active);  /* checkmark if active */
      device_map_[container] = input_devices_[i];
    }

    scroll_content()->SizeToPreferredSize();
    scroller()->Layout();
  }

  HoverHighlightView* AddScrollListItem(const string16& text,
                                        gfx::Font::FontStyle style,
                                        bool checked) {
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddCheckableLabel(text, style, checked);
    scroll_content()->AddChildView(container);
    return container;
  }

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE {
    if (sender == footer()->content()) {
      owner()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    } else {
      AudioDeviceMap::iterator iter = device_map_.find(sender);
      if (iter == device_map_.end())
        return;
      chromeos::AudioDevice& device = iter->second;
      if (device.is_input)
        chromeos::CrasAudioHandler::Get()->SetActiveInputNode(device.id);
      else
        chromeos::CrasAudioHandler::Get()->SetActiveOutputNode(device.id);
    }
  }

  typedef std::map<views::View*, chromeos::AudioDevice> AudioDeviceMap;

  user::LoginStatus login_;
  chromeos::AudioDeviceList output_devices_;
  chromeos::AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetailedView);
};

}  // namespace tray

TrayAudio::TrayAudio(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_VOLUME_MUTE),
      volume_view_(NULL),
      audio_detail_(NULL),
      pop_up_volume_view_(false) {
  if (UseNewAudioHandler())
    chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  else
    Shell::GetInstance()->system_tray_notifier()->AddAudioObserver(this);
}

TrayAudio::~TrayAudio() {
  if (UseNewAudioHandler()) {
    if (chromeos::CrasAudioHandler::IsInitialized())
      chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  } else {
    Shell::GetInstance()->system_tray_notifier()->RemoveAudioObserver(this);
  }
}

bool TrayAudio::GetInitialVisibility() {
  return IsAudioMuted();
}

views::View* TrayAudio::CreateDefaultView(user::LoginStatus status) {
  volume_view_ = new tray::VolumeView(this, true);
  return volume_view_;
}

views::View* TrayAudio::CreateDetailedView(user::LoginStatus status) {
  if (!UseNewAudioHandler() || pop_up_volume_view_) {
    volume_view_ = new tray::VolumeView(this, false);
    return volume_view_;
  } else {
    audio_detail_ = new tray::AudioDetailedView(this, status);
    return audio_detail_;
  }
}

void TrayAudio::DestroyDefaultView() {
  volume_view_ = NULL;
}

void TrayAudio::DestroyDetailedView() {
  if (audio_detail_) {
    audio_detail_ = NULL;
  } else if (volume_view_) {
    volume_view_ = NULL;
    pop_up_volume_view_ = false;
  }
}

bool TrayAudio::ShouldHideArrow() const {
  return true;
}

bool TrayAudio::ShouldShowLauncher() const {
  return false;
}

void TrayAudio::OnVolumeChanged(float percent) {
  DCHECK(!UseNewAudioHandler());
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    if (IsAudioMuted())
      percent = 0.0;
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }
  PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

void TrayAudio::OnMuteToggled() {
  DCHECK(!UseNewAudioHandler());
  if (tray_view())
      tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_)
    volume_view_->Update();
  else
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}


void TrayAudio::OnOutputVolumeChanged() {
  DCHECK(UseNewAudioHandler());
  float percent = GetVolumeLevel();
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }
  pop_up_volume_view_ = true;
  PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

void TrayAudio::OnOutputMuteChanged() {
  DCHECK(UseNewAudioHandler());
  if (tray_view())
      tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_)
    volume_view_->Update();
  else {
    pop_up_volume_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
  }
}

void TrayAudio::OnAudioNodesChanged() {
  Update();
}

void TrayAudio::OnActiveOutputNodeChanged() {
  Update();
}

void TrayAudio::OnActiveInputNodeChanged() {
  Update();
}

void TrayAudio::Update() {
  if (audio_detail_)
    audio_detail_->Update();
  if (volume_view_)
    volume_view_->Update();
}

}  // namespace internal
}  // namespace ash
