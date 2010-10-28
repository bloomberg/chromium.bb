// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/textfield_with_margin.h"

namespace {

// Holds ratio of the margin to the preferred text height.
const double kTextMarginRate = 0.33;

}  // namespace

namespace chromeos {

void TextfieldWithMargin::Layout() {
  int margin = GetPreferredSize().height() * kTextMarginRate;
  SetHorizontalMargins(margin, margin);
  views::Textfield::Layout();
}

}  // namespace chromeos
