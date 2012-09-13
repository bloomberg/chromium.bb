// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_CONTAINER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/view.h"

class LocationBarView;

namespace views {
class NativeViewHost;
}

// LocationBarContainer contains the LocationBarView. Under aura it directly
// contains the LocationBarView, on windows an intermediary widget is used.  The
// intermediary widget is used so that LocationBarContainer can be placed on top
// of other native views (such as web content).
// LocationBarContainer is positioned by ToolbarView, but it is a child of
// BrowserView. This is used when on the NTP.
class LocationBarContainer : public views::View,
                             public views::BoundsAnimatorObserver {
 public:
  // Creates a new LocationBarContainer as a child of |parent|.
  LocationBarContainer(views::View* parent, bool instant_extended_api_enabled);
  virtual ~LocationBarContainer();

  // Sets whether the LocationBarContainer is in the toolbar.
  void SetInToolbar(bool in_toolbar);

  void SetLocationBarView(LocationBarView* view);

  // Stacks this view at the top. More specifically stacks the layer (aura)
  // or widget (windows) at the top of the stack. Used to ensure this is over
  // web contents.
  void StackAtTop();

  // Animates to the specified position.
  void AnimateTo(const gfx::Rect& bounds);

  // Returns true if animating.
  bool IsAnimating() const;

  // Returns the target bounds if animating, or the actual bounds if not
  // animating.
  gfx::Rect GetTargetBounds();

  // views::View overrides:
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(
      const ui::KeyEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // views::BoundsAnimatorObserver overrides:
  virtual void OnBoundsAnimatorProgressed(
      views::BoundsAnimator* animator) OVERRIDE {}
  virtual void OnBoundsAnimatorDone(views::BoundsAnimator* animator) OVERRIDE;

 protected:
  virtual void OnFocus() OVERRIDE;

 private:
  // Sets up platform specific state.
  void PlatformInit();

  // Returns the background color.
  SkColor GetBackgroundColor();

  // Returns animation duration in milliseconds.
  static int GetAnimationDuration();

  // Used to animate this view.
  views::BoundsAnimator animator_;

  // Parent the LocationBarView is added to.
  views::View* view_parent_;

  LocationBarView* location_bar_view_;

  views::NativeViewHost* native_view_host_;

  bool in_toolbar_;

  bool instant_extended_api_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_CONTAINER_H_
