// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/volume_menu_button.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/status/status_area_bubble.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace chromeos {

namespace {

const int kMenuItemId = 100;  // arbitrary menu id.
// TODO(achuith): Minimum width of MenuItemView is 27, which is too wide.
const int kVolumeMenuWidth = 27;
const int kVolumeIconWidth = 20;

bool ShouldShowStatusAreaVolume() {
  return CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kShowVolumeStatus);
}

////////////////////////////////////////////////////////////////////////////////
// AudioHandler helpers

// Used when not running on a ChromeOS device.
static int g_volume_percent = 0;

int GetVolumePercent() {
  AudioHandler* audio_handler = AudioHandler::GetInstance();
  if (audio_handler)
    return audio_handler->IsMuted() ? 0 : audio_handler->GetVolumePercent();
  return g_volume_percent;
}

void SetVolumePercent(int percent) {
  AudioHandler* audio_handler = AudioHandler::GetInstance();
  if (audio_handler)
    audio_handler->SetVolumePercent(percent);
  g_volume_percent = percent;
}

void AddVolumeObserver(AudioHandler::VolumeObserver* volume_observer) {
  AudioHandler::GetInstance()->AddVolumeObserver(volume_observer);
}

void RemoveVolumeObserver(AudioHandler::VolumeObserver* volume_observer) {
  if (AudioHandler::GetInstance())
    AudioHandler::GetInstance()->RemoveVolumeObserver(volume_observer);
}

////////////////////////////////////////////////////////////////////////////////
// SkBitmap helpers

const SkBitmap* GetImageNamed(int image_index) {
  return ResourceBundle::GetSharedInstance().
      GetImageNamed(image_index).ToSkBitmap();
}

const SkBitmap* GetIcon() {
  const int volume_percent = GetVolumePercent();
  int image_index = IDR_STATUSBAR_VOLUME_ICON_MUTE;

  if (volume_percent > 67)
    image_index = IDR_STATUSBAR_VOLUME_ICON3;
  else if (volume_percent > 33)
    image_index = IDR_STATUSBAR_VOLUME_ICON2;
  else if (volume_percent > 0)
    image_index = IDR_STATUSBAR_VOLUME_ICON1;

  return GetImageNamed(image_index);
}

////////////////////////////////////////////////////////////////////////////////
// VolumeControlView

class VolumeControlView : public views::View,
                          public AudioHandler::VolumeObserver {
 public:
  explicit VolumeControlView(VolumeMenuButton* volume_menu_button);
  virtual ~VolumeControlView();

 private:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;

  // AudioHandler::VolumeObserver overrides:
  virtual void OnVolumeChanged() OVERRIDE;

  VolumeMenuButton* volume_menu_button_;  // not owned.

  const SkBitmap* slider_empty_;
  const SkBitmap* slider_full_;
  const SkBitmap* thumb_;

  int slider_w_;
  int slider_h_;
  int thumb_h_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControlView);
};

VolumeControlView::VolumeControlView(
    VolumeMenuButton* volume_menu_button)
    : volume_menu_button_(volume_menu_button),
      slider_empty_(GetImageNamed(IDR_STATUSBAR_VOLUME_SLIDER_EMPTY)),
      slider_full_(GetImageNamed(IDR_STATUSBAR_VOLUME_SLIDER_FULL)),
      thumb_(GetImageNamed(IDR_STATUSBAR_VOLUME_SLIDER_THUMB)),
      slider_w_(slider_empty_->width()),
      slider_h_(slider_empty_->height()),
      thumb_h_(thumb_->height()) {
  DCHECK_EQ(slider_w_, slider_full_->width());
  AddVolumeObserver(this);
}

VolumeControlView::~VolumeControlView() {
  RemoveVolumeObserver(this);
}

gfx::Size VolumeControlView::GetPreferredSize() {
  return gfx::Size(kVolumeMenuWidth, slider_h_ + thumb_h_);
}

void VolumeControlView::OnPaint(gfx::Canvas* canvas) {
  const int slider_x = (width() - slider_w_) / 2;
  const int thumb_x = (width() - thumb_->width()) / 2;
  const int slider_empty_y = thumb_->height() / 2.0;

  const int slider_empty_h = slider_h_ * (100 - GetVolumePercent()) / 100;
  const int slider_full_y = slider_empty_h + slider_empty_y;
  const int slider_full_h = slider_h_ - slider_empty_h;

  const int thumb_y = slider_empty_h;

  if (slider_empty_h != 0) {
    canvas->DrawBitmapInt(*slider_empty_,
                          0, 0, slider_w_, slider_empty_h,
                          slider_x, slider_empty_y, slider_w_, slider_empty_h,
                          false);
  }

  if (slider_full_h != 0) {
    canvas->DrawBitmapInt(*slider_full_,
                          0, slider_empty_h, slider_w_, slider_full_h,
                          slider_x, slider_full_y, slider_w_, slider_full_h,
                          false);
  }

  canvas->DrawBitmapInt(*thumb_, thumb_x, thumb_y);
}

bool VolumeControlView::OnMousePressed(const views::MouseEvent& event) {
  return OnMouseDragged(event);
}

bool VolumeControlView::OnMouseDragged(const views::MouseEvent& event) {
  const int slider_empty_y = thumb_->height() / 2.0;
  const int new_volume = 100 - (std::max(std::min((event.y() - slider_empty_y),
      slider_h_), 0) * 100 / slider_h_);
  if (new_volume != GetVolumePercent()) {
    SetVolumePercent(new_volume);
    SchedulePaint();
    volume_menu_button_->UpdateIcon();
  }
  return true;
}

void VolumeControlView::OnVolumeChanged() {
  SchedulePaint();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// VolumeMenuButton

VolumeMenuButton::VolumeMenuButton(StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, this) {
  set_id(VIEW_ID_STATUS_BUTTON_VOLUME);
  UpdateIcon();
  SetVisible(ShouldShowStatusAreaVolume());
  AddVolumeObserver(this);
}

VolumeMenuButton::~VolumeMenuButton() {
  RemoveVolumeObserver(this);
}

int VolumeMenuButton::icon_width() {
  return kVolumeIconWidth;
}

void VolumeMenuButton::UpdateIcon() {
  const int volume_percent = GetVolumePercent();
  string16 tooltip_text = (volume_percent == 0)
      ? l10n_util::GetStringUTF16(IDS_STATUSBAR_VOLUME_MUTE)
      : l10n_util::GetStringFUTF16(IDS_STATUSBAR_VOLUME_PERCENTAGE,
                                   base::IntToString16(volume_percent));
  SetTooltipText(tooltip_text);
  SetAccessibleName(tooltip_text);

  SetIcon(*GetIcon());
  SchedulePaint();
}

void VolumeMenuButton::OnLocaleChanged() {
  UpdateIcon();
}

void VolumeMenuButton::OnVolumeChanged() {
  UpdateIcon();
}

void VolumeMenuButton::OnMenuButtonClicked(views::View* source,
                                           const gfx::Point& point) {
  // TODO(achuith): Minimum width of MenuItemView is 27 pix which is too wide
  // for our purposes here.
  views::MenuItemView* menu = new views::MenuItemView(this);
  // MenuRunner takes ownership of |menu|.
  views::MenuRunner* menu_runner = new views::MenuRunner(menu);
  views::MenuItemView* submenu = menu->AppendMenuItem(
      kMenuItemId,
      string16(),
      views::MenuItemView::NORMAL);
  submenu->AddChildView(new StatusAreaBubbleContentView(
      new VolumeControlView(this), string16()));
  menu->CreateSubmenu()->set_resize_open_menu(true);
  menu->SetMargins(0, 0);
  submenu->SetMargins(0, 0);
  menu->ChildrenChanged();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());

  views::MenuRunner::RunResult result = menu_runner->RunMenuAt(
      source->GetWidget()->GetTopLevelWidget(),
      this,
      bounds,
      views::MenuItemView::TOPRIGHT,
      views::MenuRunner::HAS_MNEMONICS);

  if (result != views::MenuRunner::MENU_DELETED)
    delete menu_runner;
}

}  // namespace chromeos
