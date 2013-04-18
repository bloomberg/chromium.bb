// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// ContentsContainer is responsible for managing the WebContents views.
// ContentsContainer has up to two children: one for the currently active
// WebContents and one for Instant's WebContents.
class ContentsContainer : public views::View,
                          public content::NotificationObserver {
 public:
  // Internal class name
  static const char kViewClassName[];

  ContentsContainer(views::WebView* active, views::View* browser_view);
  virtual ~ContentsContainer();

  // Makes the overlay view the active view and nulls out the old active view.
  // The caller must delete or remove the old active view separately.
  void MakeOverlayContentsActiveContents();

  // Sets the overlay view. This does not delete the old.
  void SetOverlay(views::WebView* overlay,
                  content::WebContents* overlay_web_contents,
                  int height,
                  InstantSizeUnits units,
                  bool draw_drop_shadow);

  // When the active content is reset and we have a visible overlay,
  // the overlay must be stacked back at top.
  void MaybeStackOverlayAtTop();

  content::WebContents* overlay_web_contents() const {
    return overlay_web_contents_;
  }

  int overlay_height() const {
    return overlay_ ? overlay_->bounds().height() : 0;
  }

  // Sets the active top margin; the active WebView's y origin would be
  // positioned at this |margin|, causing the active WebView to be pushed down
  // vertically by |margin| pixels in the |ContentsContainer|. Returns true
  // if the margin changed and this view needs Layout().
  bool SetActiveTopMargin(int margin);

  // Sets a vertical offset of |margin| pixels for the |overlay_| content,
  // similar to SetActiveTopMargin(). Returns true if the margin changed and
  // this view needs Layout().
  bool SetOverlayTopMargin(int margin);

  // Returns the bounds the overlay would be shown at.
  gfx::Rect GetOverlayBounds() const;

  // Returns true if overlay will occupy full height of content page based on
  // |overlay_height| and |overlay_height_units|.
  bool WillOverlayBeFullHeight(int overlay_height,
                               InstantSizeUnits overlay_height_units) const;

  // Returns true if |overlay_| is not NULL and it is occupying full height of
  // the content page.
  bool IsOverlayFullHeight() const;

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // Testing interface:
  views::WebView* GetActiveWebViewForTest() { return active_; }
  views::WebView* GetOverlayWebViewForTest() { return overlay_; }

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  views::WebView* active_;
  views::View* browser_view_;
  views::WebView* overlay_;
  scoped_ptr<views::View> shadow_view_;
  content::WebContents* overlay_web_contents_;
  bool draw_drop_shadow_;

  // The margin between the top and the active view. This is used to make the
  // overlay overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  // The margin between the top of this view and the overlay view. Used to
  // push down the instant extended suggestions during an immersive fullscreen
  // reveal.
  int overlay_top_margin_;

  // The desired height of the overlay and units.
  int overlay_height_;
  InstantSizeUnits overlay_height_units_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
