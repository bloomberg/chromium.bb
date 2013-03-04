// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_OVERLAY_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_OVERLAY_CONTROLLER_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/instant/instant_overlay_controller.h"

class Browser;
class ContentsContainer;

namespace views {
class WebView;
}

// A controller that manages the Views-specific Instant overlay. Its primary
// role is to respond to display-state changes from the Instant model and
// reflect this in the visibility and layout of the overlay.
class InstantOverlayControllerViews : public InstantOverlayController {
 public:
  InstantOverlayControllerViews(Browser* browser, ContentsContainer* contents);
  virtual ~InstantOverlayControllerViews();

  views::WebView* overlay() { return overlay_.get(); }

  views::WebView* release_overlay() WARN_UNUSED_RESULT {
    return overlay_.release();
  }

 private:
  // Overridden from InstantOverlayController:
  virtual void OverlayStateChanged(const InstantOverlayModel& model) OVERRIDE;

  ContentsContainer* const contents_;

  // The view that contains the Instant overlay web contents.
  scoped_ptr<views::WebView> overlay_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlayControllerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_OVERLAY_CONTROLLER_VIEWS_H_
