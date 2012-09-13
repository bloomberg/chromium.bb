// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_

#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace views {
class WebView;
}

// ContentsContainer is responsible for managing the WebContents views.
// ContentsContainer has up to two children: one for the currently active
// WebContents and one for instant's WebContents.
class ContentsContainer : public views::View {
 public:
  // Internal class name
  static const char kViewClassName[];

  explicit ContentsContainer(views::WebView* active);
  virtual ~ContentsContainer();

  // Sets the active web view first in stacking order.  This view is deactivated
  // when the |SearchViewController| is displaying the NTP, and activated
  // otherwise.  Deactivation removes the active view from the view hierarchy.
  void SetActive(views::WebView* active);
  views::WebView* active() { return active_; }

  // Sets the overlay. The overlay is sized to the bounds of this view.
  void SetOverlay(views::View* overlay);
  views::View* overlay() { return overlay_; }

  // Makes the preview view the active view and nulls out the old active view.
  // It's assumed the caller will delete or remove the old active view
  // separately.
  void MakePreviewContentsActiveContents();

  // Sets the preview view. This does not delete the old.
  void SetPreview(views::WebView* preview,
                  content::WebContents* preview_web_contents);
  views::WebView* preview() { return preview_; }
  content::WebContents* preview_web_contents() const {
    return preview_web_contents_;
  }

  // Sets the active top margin.
  void SetActiveTopMargin(int margin);

  // Returns the bounds of the preview. If the preview isn't active this
  // retuns the bounds the preview would be shown at.
  gfx::Rect GetPreviewBounds();

  // View overrides:
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

 private:
  views::WebView* active_;
  views::View* overlay_;
  views::WebView* preview_;
  content::WebContents* preview_web_contents_;

  // The margin between the top and the active view. This is used to make the
  // preview overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
