// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/time_view.h"

#include "base/i18n/time_formatting.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"

namespace athena {
namespace {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

}  // namespace

TimeView::TimeView(SystemUI::ColorScheme color_scheme) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetEnabledColor((color_scheme == SystemUI::COLOR_SCHEME_LIGHT)
                      ? SK_ColorWHITE
                      : SK_ColorDKGRAY);
  SetAutoColorReadabilityEnabled(false);
  SetFontList(gfx::FontList().DeriveWithStyle(gfx::Font::BOLD));
  SetSubpixelRenderingEnabled(false);

  const int kHorizontalSpacing = 10;
  const int kVerticalSpacing = 3;
  SetBorder(views::Border::CreateEmptyBorder(kVerticalSpacing,
                                             kHorizontalSpacing,
                                             kVerticalSpacing,
                                             kHorizontalSpacing));

  UpdateText();
}

TimeView::~TimeView() {
}

void TimeView::SetTimer(base::Time now) {
  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Stop();
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(seconds_left),
      this, &TimeView::UpdateText);
}

void TimeView::UpdateText() {
  base::Time now = base::Time::Now();
  SetText(base::TimeFormatTimeOfDayWithHourClockType(
      now, base::k12HourClock, base::kKeepAmPm));
  SetTimer(now);
}

}  // namespace athena
