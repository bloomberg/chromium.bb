// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"
#include "grit/theme_resources.h"
#include "views/controls/throbber.h"

namespace chromeos {

namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

// Time in ms before smothed throbber is shown.
const int kThrobberStartDelayMs = 500;

}  // namespace

views::Throbber* CreateDefaultSmoothedThrobber() {
  views::SmoothedThrobber* throbber =
      new views::SmoothedThrobber(kThrobberFrameMs);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  throbber->set_start_delay_ms(kThrobberStartDelayMs);
  return throbber;
}

views::Throbber* CreateDefaultThrobber() {
  views::Throbber* throbber = new views::Throbber(kThrobberFrameMs, false);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  return throbber;
}

}  // namespace chromeos

