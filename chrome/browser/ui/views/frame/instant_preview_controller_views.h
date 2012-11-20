// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_PREVIEW_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_PREVIEW_CONTROLLER_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/instant/instant_preview_controller.h"

class Browser;
class ContentsContainer;

namespace views {
class WebView;
}

// A controller that manages the Views-specific Instant preview. Its primary
// role is to respond to display-state changes from the Instant model and
// reflect this in the visibility and layout of the preview.
class InstantPreviewControllerViews : public InstantPreviewController {
 public:
  InstantPreviewControllerViews(Browser* browser, ContentsContainer* contents);
  virtual ~InstantPreviewControllerViews();

  views::WebView* preview() { return preview_.get(); }

  views::WebView* release_preview() WARN_UNUSED_RESULT {
    return preview_.release();
  }

 private:
  // Overridden from InstantPreviewController:
  virtual void PreviewStateChanged(const InstantModel& model) OVERRIDE;

  ContentsContainer* const contents_;

  // The view that contains the Instant preview web contents.
  scoped_ptr<views::WebView> preview_;

  DISALLOW_COPY_AND_ASSIGN(InstantPreviewControllerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_INSTANT_PREVIEW_CONTROLLER_VIEWS_H_
