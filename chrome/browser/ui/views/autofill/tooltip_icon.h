// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/views/controls/image_view.h"

namespace autofill {

class InfoBubble;

// A tooltip icon that shows a bubble on hover. Looks like (?).
class TooltipIcon : public views::ImageView {
 public:
  explicit TooltipIcon(const base::string16& tooltip);
  virtual ~TooltipIcon();

  static const char kViewClassName[];

  // views::ImageView:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& prev_bounds) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

 private:
  // Changes this view's image to the resource indicated by |idr|.
  void ChangeImageTo(int idr);

  // Creates and shows |bubble_|. If |bubble_| already exists, just cancels a
  // potential close timer.
  void ShowBubble();

  // Hides |bubble_| if necessary.
  void HideBubble();

  // The text to show in a bubble when hovered.
  base::string16 tooltip_;

  // Whether the mouse is inside this tooltip.
  bool mouse_inside_;

  // A bubble shown on hover. Weak; owns itself. NULL while hiding.
  InfoBubble* bubble_;

  // A timer to delay hiding |bubble_|.
  base::OneShotTimer<TooltipIcon> hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(TooltipIcon);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_
