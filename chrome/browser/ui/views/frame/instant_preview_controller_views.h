// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_PREVIEW_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_PREVIEW_CONTROLLER_VIEWS_H_

#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_model_observer.h"
#include "chrome/browser/instant/instant_preview_controller.h"

class Browser;
class BrowserView;
class ContentsContainer;
class InstantModel;
class TabContents;

namespace views {
class WebView;
}

// A controller that manages the Views-specific Instant preview.  Its primary
// role is to respond to display-state changes from the Instant model and
// reflect this in the visibility and layout of the preview.
class InstantPreviewControllerViews : public InstantPreviewController {
 public:
  InstantPreviewControllerViews(Browser* browser,
                                BrowserView* browser_view,
                                ContentsContainer* contents);
  virtual ~InstantPreviewControllerViews();

  // InstantModelObserver overrides:
  virtual void DisplayStateChanged(const InstantModel& model) OVERRIDE;

  views::WebView* preview_container() { return preview_container_; }
  views::WebView* release_preview_container() {
    views::WebView* tmp = preview_container_;
    preview_container_ = NULL;
    return tmp;
  }

 private:
  void ShowInstant(TabContents* preview, int height, InstantSizeUnits units);
  void HideInstant();

  // Weak.
  BrowserView* browser_view_;

  // Weak.
  ContentsContainer* contents_;

  // The view that contains the Instant preview web contents.
  // Lazily created in ShowInstant() with ownership passed to |contents_|.
  views::WebView* preview_container_;

  DISALLOW_COPY_AND_ASSIGN(InstantPreviewControllerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_PREVIEW_CONTROLLER_VIEWS_H_
