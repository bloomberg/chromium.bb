// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/webview/webview.h"

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

namespace {

int PreviewHeightInPixels(int parent_height, int preview_height,
                          InstantSizeUnits preview_height_units) {
  preview_height = std::max(0, preview_height);
  switch (preview_height_units) {
    case INSTANT_SIZE_PERCENT:
      return std::min(parent_height, (parent_height * preview_height) / 100);

    case INSTANT_SIZE_PIXELS:
      return std::min(parent_height, preview_height);
  }
  NOTREACHED() << "unknown units: " << preview_height_units;
  return 0;
}

// This class draws the drop shadow below the preview when the non-NTP preview
// doesn't fill up the entire content page.
// This class is owned by ContentsContainer, which:
// - adds it as child when shadow is needed
// - removes it as child when shadow is not needed or when preview is nuked or
//   when preview is removed as child
// - always makes sure it's the 3rd child in the view hierarchy i.e. after
//   active and preview.
class ShadowView : public views::View {
 public:
  ShadowView() {
    drop_shadow_ = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PREVIEW_DROP_SHADOW);

    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);
    set_owned_by_client();
  }

  virtual ~ShadowView() {}

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // Only height really matters, since images will be stretched horizontally
    // across.
    return drop_shadow_.size();
  }

 protected:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    // Stretch drop shadow horizontally across.
    canvas->DrawImageInt(
        drop_shadow_,
        0, 0, drop_shadow_.width(), drop_shadow_.height(),
        0, 0, width(), drop_shadow_.height(),
        true);
  }

 private:
  gfx::ImageSkia drop_shadow_;

  DISALLOW_COPY_AND_ASSIGN(ShadowView);
};

}  // namespace

ContentsContainer::ContentsContainer(views::WebView* active)
    : active_(active),
      preview_(NULL),
      preview_web_contents_(NULL),
      draw_drop_shadow_(false),
      active_top_margin_(0),
      preview_height_(100),
      preview_height_units_(INSTANT_SIZE_PERCENT),
      extra_content_height_(0) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
  RemoveShadowView(true);
}

void ContentsContainer::MakePreviewContentsActiveContents() {
  DCHECK(preview_);

  active_ = preview_;
  preview_ = NULL;
  preview_web_contents_ = NULL;
  // Since |preview_| has been nuked, shadow view is not needed anymore.
  // Note that the previous |active_| will be deleted by caller (see
  // BrowserView::ActiveTabChanged()) after this call, hence removing the old
  // |active_| as a child, and making the new |active_| (which is the previous
  // |preview_|) the first child.
  RemoveShadowView(true);
  Layout();
}

void ContentsContainer::SetPreview(views::WebView* preview,
                                   content::WebContents* preview_web_contents,
                                   int height,
                                   InstantSizeUnits units,
                                   bool draw_drop_shadow) {
  // If drawing drop shadow, clip the bottom 1-px-thick separator out of
  // preview.
  // TODO(kuan): remove this when GWS gives chrome the height without the
  // separator.
#if !defined(OS_WIN)
  if (draw_drop_shadow)
    --height;
#endif  // !defined(OS_WIN)

  if (preview_ == preview && preview_web_contents_ == preview_web_contents &&
      preview_height_ == height && preview_height_units_ == units &&
      draw_drop_shadow_ == draw_drop_shadow) {
    return;
  }

  if (preview_ != preview) {
    if (preview_) {
      // Order of children is important: always |active_| first, then
      // |preview_|, then shadow view if necessary.  To make sure the next view
      // is added in the right order, remove shadow view every time |preview_|
      // is removed.
      RemoveShadowView(false);
      RemoveChildView(preview_);
    }
    preview_ = preview;
    if (preview_)
      AddChildView(preview_);
  }

  preview_web_contents_ = preview_web_contents;
  preview_height_ = height;
  preview_height_units_ = units;
  draw_drop_shadow_ = draw_drop_shadow;

  // Add or remove shadow view as needed.
  if (preview_ && draw_drop_shadow_) {
#if !defined(OS_WIN)
    if (!shadow_view_.get())  // Shadow view has not been created.
      shadow_view_.reset(new ShadowView());
    if (!shadow_view_->parent())  // Shadow view has not been added.
      AddChildView(shadow_view_.get());
#endif  // !defined(OS_WIN)
  } else {
    RemoveShadowView(true);
  }

  Layout();
}

void ContentsContainer::MaybeStackPreviewAtTop() {
  if (!preview_)
    return;
  // To force |preview_| to the topmost in the z-order, remove it, then add it
  // back.
  // See comments in SetPreview() for why shadow view is removed.
  bool removed_shadow = RemoveShadowView(false);
  RemoveChildView(preview_);
  AddChildView(preview_);
  if (removed_shadow)  // Add back shadow view if it was removed.
    AddChildView(shadow_view_.get());
  Layout();
}

void ContentsContainer::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return;

  active_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
}

gfx::Rect ContentsContainer::GetPreviewBounds() const {
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

void ContentsContainer::SetExtraContentHeight(int height) {
  if (height == extra_content_height_)
    return;
  extra_content_height_ = height;
}

bool ContentsContainer::IsPreviewFullHeight(
    int preview_height,
    InstantSizeUnits preview_height_units) const {
  int height_in_pixels = PreviewHeightInPixels(height(), preview_height,
                                               preview_height_units);
  return height_in_pixels == height();
}

void ContentsContainer::Layout() {
  int content_y = active_top_margin_;
  int content_height =
      std::max(0, height() - content_y + extra_content_height_);

  active_->SetBounds(0, content_y, width(), content_height);

  if (preview_) {
    preview_->SetBounds(0, 0, width(),
                        PreviewHeightInPixels(height(), preview_height_,
                                              preview_height_units_));
    if (draw_drop_shadow_) {
#if !defined(OS_WIN)
      DCHECK(shadow_view_.get() && shadow_view_->parent());
      shadow_view_->SetBounds(0, preview_->bounds().height(), width(),
                              shadow_view_->GetPreferredSize().height());
#endif  // !defined(OS_WIN)
    }
  }

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

bool ContentsContainer::RemoveShadowView(bool delete_view) {
  if (!shadow_view_.get())
    return false;

  if (!shadow_view_->parent()) {
    if (delete_view)
      shadow_view_.reset(NULL);
    return false;
  }

  RemoveChildView(shadow_view_.get());
  if (delete_view)
    shadow_view_.reset(NULL);
  return true;
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
}
