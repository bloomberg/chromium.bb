// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/instant_types.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace views {
class WebView;
}

// ContentsContainer is responsible for managing the WebContents views.
// ContentsContainer has up to two children: one for the currently active
// WebContents and one for Instant's WebContents.
class ContentsContainer : public views::View {
 public:
  // Internal class name
  static const char kViewClassName[];

  explicit ContentsContainer(views::WebView* active);
  virtual ~ContentsContainer();

  // Makes the preview view the active view and nulls out the old active view.
  // The caller must delete or remove the old active view separately.
  void MakePreviewContentsActiveContents();

  // Sets the preview view. This does not delete the old.
  void SetPreview(views::WebView* preview,
                  content::WebContents* preview_web_contents,
                  int height,
                  InstantSizeUnits units);

  // When the active content is reset and we have a visible preview,
  // the preview must be stacked back at top.
  void MaybeStackPreviewAtTop();

  content::WebContents* preview_web_contents() const {
    return preview_web_contents_;
  }

  // Sets the active top margin.
  void SetActiveTopMargin(int margin);

  // Returns the bounds the preview would be shown at.
  gfx::Rect GetPreviewBounds() const;

  // Set/Get an extra content height, so that room is left at the bottom of the
  // contents view for other views to draw on top of the extended child web
  // view. Note that this doesn't cause a layout invalidation, it's up to the
  // caller to make sure that a Layout() will be done so that the
  // |extra_content_height_| gets taken into account.
  int extra_content_height() const { return extra_content_height_; }
  void SetExtraContentHeight(int height);

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // Returns |preview_height_| in pixels.
  int PreviewHeightInPixels() const;

  views::WebView* active_;
  views::WebView* preview_;
  content::WebContents* preview_web_contents_;

  // The margin between the top and the active view. This is used to make the
  // preview overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  // The desired height of the preview and units.
  int preview_height_;
  InstantSizeUnits preview_height_units_;

  // Used to extend the child WebView beyond the contents view bottom bound.
  int extra_content_height_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
