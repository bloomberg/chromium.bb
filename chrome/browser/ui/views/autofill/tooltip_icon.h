// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/views/controls/image_view.h"

// A tooltip icon (just an ImageView with a tooltip). Looks like (?).
class TooltipIcon : public views::ImageView {
 public:
  explicit TooltipIcon(const base::string16& tooltip);
  virtual ~TooltipIcon();

  // views::View implementation
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE;
 private:
  base::string16 tooltip_;

  DISALLOW_COPY_AND_ASSIGN(TooltipIcon);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_
