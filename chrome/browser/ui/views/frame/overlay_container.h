// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OVERLAY_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OVERLAY_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/view.h"

class BrowserView;
class ImmersiveModeController;
class ImmersiveRevealedLock;

namespace content {
class WebContents;
}

namespace gfx {
class Canvas;
class Rect;
}

namespace views {
class WebView;
};

// OverlayContainer is responsible for managing the overlay WebContents view.
// OverlayContainer has up to two children: one for Instant's WebContents and,
// if necessary, one for the drop shadow below it in the y-axis.
class OverlayContainer : public views::View,
                         public content::NotificationObserver {
 public:
  OverlayContainer(BrowserView* browser_view,
                   ImmersiveModeController* immersive_mode_controller);
  virtual ~OverlayContainer();

  // Sets the overlay view. This does not delete the old.
  void SetOverlay(views::WebView* overlay,
                  content::WebContents* overlay_web_contents,
                  int height,
                  InstantSizeUnits units,
                  bool draw_drop_shadow);

  // Called after overlay view has been reparented to |ContentsContainer| and
  // has been made the active view.
  void ResetOverlayAndContents();

  // Returns the bounds the overlay would be shown at.
  gfx::Rect GetOverlayBounds() const;

  // Returns true if overlay will occupy full height of available space based on
  // |overlay_height| and |overlay_height_units|.
  bool WillOverlayBeFullHeight(int overlay_height,
                               InstantSizeUnits overlay_height_units) const;

  // Returns true if |overlay_| is not NULL and it is occupying full height of
  // available space.  Must be called before GetPreferredSize(), and only call
  // the latter if this returns false.
  bool IsOverlayFullHeight() const;

  content::WebContents* overlay_web_contents() const {
    return overlay_web_contents_;
  }

  // Overridden from views::View:
  // GetPreferredSize() must only be called if IsOverlayFullHeight() returns
  // false; if overlay is full height, height of OverlayContainer can only be
  // determined by the parent's height, not here.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // Testing interface:
  views::WebView* GetOverlayWebViewForTest() { return overlay_; }

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  BrowserView* const browser_view_;
  ImmersiveModeController* immersive_mode_controller_;

  // Used to force the top views open while in immersive fullscreen.
  scoped_ptr<ImmersiveRevealedLock> immersive_revealed_lock_;

  // Owned by |InstantOverlayControllerViews|.
  views::WebView* overlay_;

  // Owned by |IntantController| if not NULL, else |InstantModel|.
  content::WebContents* overlay_web_contents_;

  // Drop shadow below partial-height |overlay_|.
  scoped_ptr<views::View> shadow_view_;
  bool draw_drop_shadow_;

  // The desired height of the overlay and units.
  int overlay_height_;
  InstantSizeUnits overlay_height_units_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OverlayContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OVERLAY_CONTAINER_H_
