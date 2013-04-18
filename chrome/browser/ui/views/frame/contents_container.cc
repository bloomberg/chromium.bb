// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

namespace {

int OverlayHeightInPixels(int parent_height, int overlay_height,
                          InstantSizeUnits overlay_height_units) {
  overlay_height = std::max(0, overlay_height);
  switch (overlay_height_units) {
    case INSTANT_SIZE_PERCENT:
      return std::min(parent_height, (parent_height * overlay_height) / 100);

    case INSTANT_SIZE_PIXELS:
      return std::min(parent_height, overlay_height);
  }
  NOTREACHED() << "unknown units: " << overlay_height_units;
  return 0;
}

// This class draws the drop shadow below the overlay when the non-NTP overlay
// doesn't fill up the entire content page.
// This class is owned by ContentsContainer, which:
// - adds it as child when shadow is needed
// - removes it as child when shadow is not needed or when overlay is nuked or
//   when overlay is removed as child
// - always makes sure it's the 3rd child in the view hierarchy i.e. after
//   active and overlay.
class ShadowView : public views::View {
 public:
  ShadowView() {
    drop_shadow_ = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_OVERLAY_DROP_SHADOW);

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

ContentsContainer::ContentsContainer(views::WebView* active,
                                     views::View* browser_view)
    : active_(active),
      browser_view_(browser_view),
      overlay_(NULL),
      overlay_web_contents_(NULL),
      draw_drop_shadow_(false),
      active_top_margin_(0),
      overlay_top_margin_(0),
      overlay_height_(100),
      overlay_height_units_(INSTANT_SIZE_PERCENT) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
}

void ContentsContainer::MakeOverlayContentsActiveContents() {
  DCHECK(overlay_);

  active_ = overlay_;
  overlay_ = NULL;
  overlay_web_contents_ = NULL;
  // Unregister from observing previous |overlay_web_contents_|.
  registrar_.RemoveAll();
  // Since |overlay_| has been nuked, shadow view is not needed anymore.
  // Note that the previous |active_| will be deleted by caller (see
  // BrowserView::ActiveTabChanged()) after this call, hence removing the old
  // |active_| as a child, and making the new |active_| (which is the previous
  // |overlay_|) the first child.
  shadow_view_.reset();
  Layout();
}

void ContentsContainer::SetOverlay(views::WebView* overlay,
                                   content::WebContents* overlay_web_contents,
                                   int height,
                                   InstantSizeUnits units,
                                   bool draw_drop_shadow) {
  // If drawing drop shadow, clip the bottom 1-px-thick separator out of
  // overlay.
  // TODO(kuan): remove this when GWS gives chrome the height without the
  // separator.
#if !defined(OS_WIN)
  if (draw_drop_shadow)
    --height;
#endif  // !defined(OS_WIN)

  if (overlay_ == overlay && overlay_web_contents_ == overlay_web_contents &&
      overlay_height_ == height && overlay_height_units_ == units &&
      draw_drop_shadow_ == draw_drop_shadow) {
    return;
  }

  if (overlay_ != overlay) {
    if (overlay_) {
      // Order of children is important: always |active_| first, then
      // |overlay_|, then shadow view if necessary.  To make sure the next view
      // is added in the right order, remove shadow view every time |overlay_|
      // is removed. Don't nuke the shadow view now in case it's needed below
      // when we handle |draw_drop_shadow|.
      if (shadow_view_.get())
        RemoveChildView(shadow_view_.get());
      RemoveChildView(overlay_);
    }
    overlay_ = overlay;
    if (overlay_)
      AddChildView(overlay_);
  }

  if (overlay_web_contents_ != overlay_web_contents) {
#if !defined(OS_WIN)
    // Unregister from observing previous |overlay_web_contents_|.
    registrar_.RemoveAll();
#endif  // !defined(OS_WIN)

    overlay_web_contents_ = overlay_web_contents;

#if !defined(OS_WIN)
    // Register to new overlay web contents' render view host.
    if (overlay_web_contents_) {
      content::RenderViewHost* rvh = overlay_web_contents_->GetRenderViewHost();
      DCHECK(rvh);
      if (rvh) {
        content::NotificationSource source =
            content::Source<content::RenderWidgetHost>(rvh);
        registrar_.Add(this,
            content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
            source);
      }
    }
#endif  // !defined(OS_WIN)
  }

  // If |overlay_|'s height has shrunk and |active_top_margin_| was used to
  // preserve |active_|'s origin in BrowserViewLayout::Layout(), we need to re-
  // determine if its origin still needs to be preserved.  The origin is
  // preserved if overlay is taller than total height of hidden bookmark and
  // info bars.  In this case, force a re-layout of BrowserView.
  bool layout_browser_view = false;
  if (overlay_ && active_top_margin_ > 0 && units == INSTANT_SIZE_PIXELS &&
      height > 0 && height < overlay_height_) {
    layout_browser_view = true;
  }

  overlay_height_ = height;
  overlay_height_units_ = units;
  draw_drop_shadow_ = draw_drop_shadow;

  // Add shadow view if there's overlay and drop shadow is needed.
  // Remove shadow view if there's no overlay.
  // If there's overlay and drop shadow is not needed, that means the partial-
  // height overlay is going to be full-height.  Don't remove the shadow view
  // yet because its view will disappear noticeably faster than the webview-ed
  // overlay is repainted at the full height - when resizing web contents page,
  // RenderWidgetHostViewAura locks the compositor until texture is updated or
  // timeout occurs.  This out-of-sync refresh results in a split second where
  // there's no separator between the overlay and active contents, making the
  // overlay contents erroneously appear to be part of active contents.
  // When the overlay is repainted at the full height, we'll be notified via
  // NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKGING_STORE, at which time
  // the shadow view will be removed.
  if (overlay_ && draw_drop_shadow_) {
#if !defined(OS_WIN)
    if (!shadow_view_.get())  // Shadow view has not been created.
      shadow_view_.reset(new ShadowView());
    if (!shadow_view_->parent())  // Shadow view has not been added.
      AddChildView(shadow_view_.get());
#endif  // !defined(OS_WIN)
  } else if (!overlay_) {
    shadow_view_.reset();
  }

  if (layout_browser_view)
    browser_view_->Layout();  // This will trigger |this| Layout.
  else
    Layout();
}

void ContentsContainer::MaybeStackOverlayAtTop() {
  if (!overlay_)
    return;
  // To force |overlay_| to the topmost in the z-order, remove it, then add it
  // back.
  // See comments in SetOverlay() for why shadow view is removed.
  bool removed_shadow = false;
  if (shadow_view_.get()) {
    RemoveChildView(shadow_view_.get());
    removed_shadow = true;
  }
  RemoveChildView(overlay_);
  AddChildView(overlay_);
  if (removed_shadow)  // Add back shadow view if it was removed.
    AddChildView(shadow_view_.get());
  Layout();
}

bool ContentsContainer::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return false;

  active_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
  return true;
}

bool ContentsContainer::SetOverlayTopMargin(int margin) {
  if (overlay_top_margin_ == margin)
    return false;
  overlay_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
  return true;
}

gfx::Rect ContentsContainer::GetOverlayBounds() const {
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

bool ContentsContainer::WillOverlayBeFullHeight(
    int overlay_height,
    InstantSizeUnits overlay_height_units) const {
  int height_in_pixels = OverlayHeightInPixels(height(), overlay_height,
                                               overlay_height_units);
  return height_in_pixels == height();
}

bool ContentsContainer::IsOverlayFullHeight() const {
  return overlay_ && overlay_height_ == 100 &&
      overlay_height_units_ == INSTANT_SIZE_PERCENT;
}

void ContentsContainer::Layout() {
  int content_y = active_top_margin_;
  int content_height = std::max(0, height() - content_y);

  active_->SetBounds(0, content_y, width(), content_height);

  if (overlay_) {
    int target_overlay_height =
        OverlayHeightInPixels(height(), overlay_height_, overlay_height_units_);
    // Ensure the overlay doesn't extend outside this container view.
    int overlay_height =
        std::min(height() - overlay_top_margin_, target_overlay_height);
    overlay_->SetBounds(0, overlay_top_margin_,
                        width(), std::max(0, overlay_height));
    if (draw_drop_shadow_) {
#if !defined(OS_WIN)
      DCHECK(shadow_view_.get() && shadow_view_->parent());
      shadow_view_->SetBounds(0, overlay_->bounds().bottom(), width(),
                              shadow_view_->GetPreferredSize().height());
#endif  // !defined(OS_WIN)
    }
  }

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
}

void ContentsContainer::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
            type);
  // Remove shadow view if it's not needed.
  if (overlay_ && !draw_drop_shadow_) {
    DCHECK(overlay_web_contents_);
    DCHECK_EQ(overlay_web_contents_->GetRenderViewHost(),
              (content::Source<content::RenderWidgetHost>(source)).ptr());
    shadow_view_.reset();
  }
}
