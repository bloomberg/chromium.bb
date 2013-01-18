// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"
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
                  const chrome::search::Mode& search_mode,
                  int height,
                  InstantSizeUnits units,
                  bool draw_drop_shadow);

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

  // Returns true if preview occupies full height of content page.
  bool IsPreviewFullHeight(int preview_height,
                           InstantSizeUnits preview_height_units) const;

 private:
  // Returns true if |shadow_view_| was a child of |ContentsContainer| and
  // successfully removed from view hierarchy.
  // If |delete_view| is true, |shadow_view_| is deleted regardless if it is a
  // child of |ContentsContainer|.
  bool RemoveShadowView(bool delete_view);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  views::WebView* active_;
  views::WebView* preview_;
  scoped_ptr<views::View> shadow_view_;
  content::WebContents* preview_web_contents_;
  chrome::search::Mode search_mode_;
  bool draw_drop_shadow_;

  // The margin between the top and the active view. This is used to make the
  // preview overlap the bookmark bar on the new tab page.
  int active_top_margin_;

  // The desired height of the preview and units.
  int preview_height_;
  InstantSizeUnits preview_height_units_;

  DISALLOW_COPY_AND_ASSIGN(ContentsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_H_
