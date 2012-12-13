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

namespace internal {
class ShadowContainer;
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
                  InstantSizeUnits units,
                  bool draw_drop_shadow);

  // When the active content is reset and we have a visible preview,
  // the preview must be stacked back at top.
  void MaybeStackPreviewAtTop();

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

  // Returns true if |this| has the same preview contents as |contents|.
  bool HasSamePreviewContents(content::WebContents* contents) const;

  // Returns true if preview occupies full height of content page.
  bool IsPreviewFullHeight(int preview_height,
                           InstantSizeUnits preview_height_units) const;

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  views::WebView* active_;

  // Contains the child preview, and is responsible for drawing the bottom
  // edge and drop shadow below the preview when necessary.
  internal::ShadowContainer* shadow_container_;

  // The margin between the top and the active view. This is used to make the
  // preview overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  // Used to extend the child WebView beyond the contents view bottom bound.
  int extra_content_height_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
