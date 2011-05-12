// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_view.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/background.h"
#include "views/widget/widget.h"

namespace {

// When we are animating, we draw only the top part of the left and right
// edges to give the illusion that the find dialog is attached to the
// window during this animation; this is the height of the items we draw.
const int kAnimatingEdgeHeight = 5;

}  // namespace

DropdownBarView::DropdownBarView(DropdownBarHost* host)
    : dialog_left_(NULL),
      dialog_middle_(NULL),
      dialog_right_(NULL),
      host_(host),
      animation_offset_(0) {
}

DropdownBarView::~DropdownBarView() {
}

////////////////////////////////////////////////////////////////////////////////
// DropDownBarView, public:

void DropdownBarView::SetAnimationOffset(int offset) {
  animation_offset_ = offset;
}

////////////////////////////////////////////////////////////////////////////////
// DropDownBarView, protected:

void DropdownBarView::SetDialogBorderBitmaps(const SkBitmap* dialog_left,
                                             const SkBitmap* dialog_middle,
                                             const SkBitmap* dialog_right) {
  dialog_left_ = dialog_left;
  dialog_middle_ = dialog_middle;
  dialog_right_ = dialog_right;
}

gfx::Rect DropdownBarView::PaintOffsetToolbarBackground(gfx::Canvas* canvas) {
  // Determine the find bar size as well as the offset from which to tile the
  // toolbar background image.  First, get the widget bounds.
  gfx::Rect bounds = GetWidget()->GetWindowScreenBounds();
  // Now convert from screen to parent coordinates.
  gfx::Point origin(bounds.origin());
  BrowserView* browser_view = host()->browser_view();
  ConvertPointToView(NULL, browser_view, &origin);
  bounds.set_origin(origin);
  // Finally, calculate the background image tiling offset.
  origin = browser_view->OffsetPointForToolbarBackgroundImage(origin);

  // First, we draw the background image for the whole dialog (3 images: left,
  // middle and right). Note, that the window region has been set by the
  // controller, so the whitespace in the left and right background images is
  // actually outside the window region and is therefore not drawn. See
  // FindInPageWidgetWin::CreateRoundedWindowEdges() for details.
  ui::ThemeProvider* tp = GetThemeProvider();
  canvas->TileImageInt(*tp->GetBitmapNamed(IDR_THEME_TOOLBAR), origin.x(),
      origin.y(), 0, 0, bounds.width(), bounds.height());
  return bounds;
}

void DropdownBarView::PaintDialogBorder(gfx::Canvas* canvas,
                                        const gfx::Rect& bounds) const {
  DCHECK(dialog_left_);
  DCHECK(dialog_middle_);
  DCHECK(dialog_right_);

  canvas->DrawBitmapInt(*dialog_left_, 0, 0);

  // Stretch the middle background to cover all of the area between the two
  // other images.
  canvas->TileImageInt(*dialog_middle_, dialog_left_->width(), 0,
      bounds.width() - dialog_left_->width() - dialog_right_->width(),
      dialog_middle_->height());

  canvas->DrawBitmapInt(*dialog_right_, bounds.width() - dialog_right_->width(),
      0);
}

void DropdownBarView::PaintAnimatingEdges(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) const {
  if (animation_offset() > 0) {
    // While animating we draw the curved edges at the point where the
    // controller told us the top of the window is: |animation_offset()|.
    canvas->TileImageInt(*dialog_left_, bounds.x(), animation_offset(),
                         dialog_left_->width(), kAnimatingEdgeHeight);
    canvas->TileImageInt(*dialog_right_,
        bounds.width() - dialog_right_->width(), animation_offset(),
        dialog_right_->width(), kAnimatingEdgeHeight);
  }
}
