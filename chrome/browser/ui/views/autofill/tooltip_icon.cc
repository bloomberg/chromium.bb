// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/tooltip_icon.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

TooltipIcon::TooltipIcon(const base::string16& tooltip) : tooltip_(tooltip) {
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AUTOFILL_TOOLTIP_ICON).ToImageSkia());
}

TooltipIcon::~TooltipIcon() {}

bool TooltipIcon::GetTooltipText(const gfx::Point& p,
                                 base::string16* tooltip) const {
  *tooltip = tooltip_;
  return !tooltip_.empty();
}
