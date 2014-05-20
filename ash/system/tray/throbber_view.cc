// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/throbber_view.h"

#include "ash/system/tray/tray_constants.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 30;

// Duration for showing/hiding animation in milliseconds.
const int kThrobberAnimationDurationMs = 200;

}  // namespace

SystemTrayThrobber::SystemTrayThrobber(int frame_delay_ms)
    : views::SmoothedThrobber(frame_delay_ms) {
}

SystemTrayThrobber::~SystemTrayThrobber() {
}

void SystemTrayThrobber::SetTooltipText(const base::string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool SystemTrayThrobber::GetTooltipText(const gfx::Point& p,
                                        base::string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

ThrobberView::ThrobberView() {
  throbber_ = new SystemTrayThrobber(kThrobberFrameMs);
  throbber_->SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_CROS_DEFAULT_THROBBER).ToImageSkia());
  throbber_->set_stop_delay_ms(kThrobberAnimationDurationMs);
  AddChildView(throbber_);

  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.0);
}

ThrobberView::~ThrobberView() {
}

gfx::Size ThrobberView::GetPreferredSize() const {
  return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
}

void ThrobberView::Layout() {
  View* child = child_at(0);
  gfx::Size ps = child->GetPreferredSize();
  child->SetBounds((width() - ps.width()) / 2,
                   (height() - ps.height()) / 2,
                   ps.width(), ps.height());
  SizeToPreferredSize();
}

bool ThrobberView::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

void ThrobberView::Start() {
  ScheduleAnimation(true);
  throbber_->Start();
}

void ThrobberView::Stop() {
  ScheduleAnimation(false);
  throbber_->Stop();
}

void ThrobberView::SetTooltipText(const base::string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
  throbber_->SetTooltipText(tooltip_text);
}

void ThrobberView::ScheduleAnimation(bool start_throbber) {
  // Stop any previous animation.
  layer()->GetAnimator()->StopAnimating();

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kThrobberAnimationDurationMs));

  layer()->SetOpacity(start_throbber ? 1.0 : 0.0);
}

}  // namespace ash
