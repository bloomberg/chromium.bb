// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/tooltip_icon.h"

#include "base/basictypes.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/autofill/info_bubble.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace autofill {

namespace {

// An info bubble with some extra positioning magic for tooltip icons.
class TooltipBubble : public InfoBubble {
 public:
  TooltipBubble(views::View* anchor, const base::string16& message)
      : InfoBubble(anchor, message) {}
  virtual ~TooltipBubble() {}

 protected:
  // InfoBubble:
  virtual gfx::Rect GetAnchorRect() OVERRIDE {
    gfx::Size pref_size = anchor()->GetPreferredSize();
    gfx::Point origin = anchor()->GetBoundsInScreen().CenterPoint();
    origin.Offset(-pref_size.width() / 2, -pref_size.height() / 2);
    return gfx::Rect(origin, pref_size);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TooltipBubble);
};

}  // namespace

TooltipIcon::TooltipIcon(const base::string16& tooltip)
    : tooltip_(tooltip),
      bubble_(NULL) {
  ChangeImageTo(IDR_AUTOFILL_TOOLTIP_ICON);
}

TooltipIcon::~TooltipIcon() {
  HideBubble();
}

// static
const char TooltipIcon::kViewClassName[] = "autofill/TooltipIcon";

const char* TooltipIcon::GetClassName() const {
  return TooltipIcon::kViewClassName;
}

void TooltipIcon::OnMouseEntered(const ui::MouseEvent& event) {
  ChangeImageTo(IDR_AUTOFILL_TOOLTIP_ICON_H);
  ShowBubble();
}

void TooltipIcon::OnMouseExited(const ui::MouseEvent& event) {
  ChangeImageTo(IDR_AUTOFILL_TOOLTIP_ICON);
  hide_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(250), this,
                    &TooltipIcon::HideBubble);
}

void TooltipIcon::ChangeImageTo(int idr) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(rb.GetImageNamed(idr).ToImageSkia());
}

void TooltipIcon::ShowBubble() {
  if (bubble_) {
    hide_timer_.Stop();
  } else {
    bubble_ = new TooltipBubble(this, tooltip_);
    bubble_->Show();
  }
}

void TooltipIcon::HideBubble() {
  if (bubble_) {
    bubble_->Hide();
    bubble_ = NULL;
  }
}

}  // namespace autofill
