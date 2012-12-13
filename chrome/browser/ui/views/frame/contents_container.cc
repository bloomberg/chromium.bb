// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/webview/webview.h"

namespace {

int PreviewHeightInPixels(int parent_height, int preview_height,
                          InstantSizeUnits preview_height_units) {
  switch (preview_height_units) {
    case INSTANT_SIZE_PERCENT:
      return std::min(parent_height, (parent_height * preview_height) / 100);

    case INSTANT_SIZE_PIXELS:
      return std::min(parent_height, preview_height);
  }
  NOTREACHED() << "unknown units: " << preview_height_units;
  return 0;
}

}  // namespace

namespace internal {

// This class serves as a container for |ContentsContainer|'s preview to draw
// a drop shadow below the preview when the preview is not for |NTP| and does
// not fill up the entire contents page.
class ShadowContainer : public views::View {
 public:
  ShadowContainer()
      : preview_(NULL),
        preview_web_contents_(NULL),
        draw_drop_shadow_(false),
        preview_height_(100),
        preview_height_units_(INSTANT_SIZE_PERCENT) {
    border_ = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PREVIEW_BORDER);
    drop_shadow_ = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PREVIEW_DROP_SHADOW);
  }

  virtual ~ShadowContainer() {}

  // Returns true if preview has been set because at least one parameter is
  // different from current.
  bool SetPreview(views::WebView* preview,
                  content::WebContents* preview_web_contents,
                  int height,
                  InstantSizeUnits units,
                  bool draw_drop_shadow) {
    // If drawing drop shadow, clip the bottom 1-px-thick separator out of
    // preview.
    // TODO(kuan): remove this when GWS gives chrome the height without the
    // separator.
    if (draw_drop_shadow)
      --height;

    if (preview_ == preview && preview_web_contents_ == preview_web_contents &&
        preview_height_ == height && preview_height_units_ == units &&
        draw_drop_shadow_ == draw_drop_shadow) {
      return false;
    }

    if (preview_)
      RemoveChildView(preview_);
    preview_ = preview;
    if (preview_)
      AddChildView(preview_);

    preview_web_contents_ = preview_web_contents;
    preview_height_ = height;
    preview_height_units_ = units;
    draw_drop_shadow_ = draw_drop_shadow;

    SetPaintToLayer(draw_drop_shadow_);
    SetFillsBoundsOpaquely(false);

    return true;
  }

  views::WebView* ReleasePreview() {
    DCHECK(preview_);
    views::WebView* preview = preview_;
    preview_ = NULL;
    preview_web_contents_ = NULL;
    return preview;
  }

  void SetPreviewBounds(int parent_width, int parent_height) {
    if (!preview_)
      return;

    // Height includes drop shadow's height if drawing drop shadow.
    SetBounds(0, 0, parent_width,
              PreviewHeightInPixels(parent_height, preview_height_,
                                    preview_height_units_) +
                  (draw_drop_shadow_ ? drop_shadow_.height() : 0));
  }

  // Returns true if operation is successful.
  bool MaybeStackPreviewAtTop() {
    if (!preview_)
      return false;
    // To force |preview_| to the topmost in the z-order, remove it, then add it
    // back.
    RemoveChildView(preview_);
    AddChildView(preview_);
    return true;
  }

  // For access by |ContentsContainer|.
  content::WebContents* preview_web_contents() const {
    return preview_web_contents_;
  }

  // views::View implementation:
  virtual void Layout() OVERRIDE {
    if (!preview_)
      return;
    // Preview takes up full height of |ShadowContainer| if not drawing drop
    // shadow.
    preview_->SetBounds(0, 0, width(),
        std::max(0,
                 height() - (draw_drop_shadow_ ? drop_shadow_.height() : 0)));
  }

  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE {
    // If necessary, paint bottom border and drop shadow.
    if (preview_ && draw_drop_shadow_) {
      // Stretch 1-px thick border horizontally across.
      canvas->DrawImageInt(
          border_,
          0, 0, border_.width(), border_.height(),
          0, height() - drop_shadow_.height(), width(), border_.height(),
          true);
      // Stretch drop shadow horizontally across, where top pixel of drop shadow
      // overlaps with 1-px thick border.
      canvas->DrawImageInt(
          drop_shadow_,
          0, 0, drop_shadow_.width(), drop_shadow_.height(),
          0, height() - drop_shadow_.height(), width(), drop_shadow_.height(),
          true);
    }
  }

 private:
  views::WebView* preview_;
  content::WebContents* preview_web_contents_;
  bool draw_drop_shadow_;

  // The desired height of the preview and units.
  int preview_height_;
  InstantSizeUnits preview_height_units_;

  gfx::ImageSkia border_;
  gfx::ImageSkia drop_shadow_;

  DISALLOW_COPY_AND_ASSIGN(ShadowContainer);
};

}  // namespace internal

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

ContentsContainer::ContentsContainer(views::WebView* active)
    : active_(active),
      shadow_container_(new internal::ShadowContainer()),
      active_top_margin_(0),
      extra_content_height_(0) {
  AddChildView(active_);
  AddChildView(shadow_container_);
}

ContentsContainer::~ContentsContainer() {
}

void ContentsContainer::MakePreviewContentsActiveContents() {
  active_ = shadow_container_->ReleasePreview();
  Layout();
}

void ContentsContainer::SetPreview(views::WebView* preview,
                                   content::WebContents* preview_web_contents,
                                   int height,
                                   InstantSizeUnits units,
                                   bool draw_drop_shadow) {
  if (shadow_container_->SetPreview(preview, preview_web_contents, height,
                                    units, draw_drop_shadow)) {
    Layout();
  }
}

void ContentsContainer::MaybeStackPreviewAtTop() {
  if (shadow_container_->MaybeStackPreviewAtTop())
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
  // Preview's origin is origin of |ContentsContainer|.
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

void ContentsContainer::SetExtraContentHeight(int height) {
  if (height == extra_content_height_)
    return;
  extra_content_height_ = height;
}

bool ContentsContainer::HasSamePreviewContents(
    content::WebContents* contents) const {
  return shadow_container_->preview_web_contents() == contents;
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

  shadow_container_->SetPreviewBounds(width(), height());

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
}
