// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ADD_TO_APP_LAUNCHER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ADD_TO_APP_LAUNCHER_VIEW_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

class LocationBarView;

namespace content {
class WebContents;
}

namespace gfx {
class FontList;
}

namespace views {
class ImageView;
class Label;
}

// The AddToAppLauncherView displays a UI in the location bar to allow adding
// the current page to the App Launcher.
class AddToAppLauncherView : public gfx::AnimationDelegate, public views::View {
 public:
  AddToAppLauncherView(LocationBarView* parent,
                       const gfx::FontList& font_list,
                       SkColor text_color,
                       SkColor parent_background_color);
  virtual ~AddToAppLauncherView();

  // Updates the decoration from the shown WebContents.
  void Update(content::WebContents* web_contents);

 private:
  // gfx::AnimationDelegate:
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE;

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  LocationBarView* parent_;  // Weak, owns us.
  scoped_ptr<views::Painter> background_painter_;
  views::ImageView* icon_;
  views::Label* text_label_;
  gfx::SlideAnimation slide_animator_;

  DISALLOW_COPY_AND_ASSIGN(AddToAppLauncherView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ADD_TO_APP_LAUNCHER_VIEW_H_
