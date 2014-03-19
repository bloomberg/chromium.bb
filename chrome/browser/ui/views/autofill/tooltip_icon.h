// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/widget/widget_observer.h"

namespace autofill {

class InfoBubble;

// A tooltip icon that shows a bubble on hover. Looks like (?).
class TooltipIcon : public views::ImageView,
                    public views::MouseWatcherListener,
                    public views::WidgetObserver {
 public:
  static const char kViewClassName[];

  explicit TooltipIcon(const base::string16& tooltip);
  virtual ~TooltipIcon();

  // views::ImageView:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // views::MouseWatcherListener:
  virtual void MouseMovedOutOfHost() OVERRIDE;

  // views::WidgetObserver:
  virtual void OnWidgetDestroyed(views::Widget* widget) OVERRIDE;

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

  // A timer to delay showing |bubble_|.
  base::OneShotTimer<TooltipIcon> show_timer_;

  // A watcher that keeps |bubble_| open if the user's mouse enters it.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  ScopedObserver<views::Widget, TooltipIcon> observer_;

  DISALLOW_COPY_AND_ASSIGN(TooltipIcon);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_TOOLTIP_ICON_H_
