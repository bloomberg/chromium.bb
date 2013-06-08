// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/overlay_container.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/detachable_toolbar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/webview/webview.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // USE_AURA

namespace {

const int kToolbarOverlap = views::NonClientFrameView::kClientEdgeThickness +
                            BookmarkBarView::kToolbarAttachedBookmarkBarOverlap;

bool IsFullHeight(int height, InstantSizeUnits units) {
  return height == 100 && units == INSTANT_SIZE_PERCENT;
}

int OverlayHeightInPixels(int parent_height, int overlay_height,
                          InstantSizeUnits overlay_height_units) {
  parent_height = std::max(0, parent_height);
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
// This class is owned by OverlayContainer, which:
// - adds it as child when shadow is needed
// - removes it as child when shadow is not needed or when overlay is nuked or
//   when overlay is removed as child
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

  virtual const char* GetClassName() const OVERRIDE {
    return "OverlayDropShadow";
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

OverlayContainer::OverlayContainer(
    BrowserView* browser_view,
    ImmersiveModeController* immersive_mode_controller)
    : browser_view_(browser_view),
      immersive_mode_controller_(immersive_mode_controller),
      overlay_(NULL),
      overlay_web_contents_(NULL),
      draw_drop_shadow_(false),
      overlay_height_(0),
      overlay_height_units_(INSTANT_SIZE_PIXELS) {
}

OverlayContainer::~OverlayContainer() {
}

void OverlayContainer::ResetOverlayAndContents() {
  // Since |overlay_| will be nuked, shadow view is not needed anymore.
  shadow_view_.reset();
  overlay_ = NULL;
  overlay_web_contents_ = NULL;
  // Allow top views to close in immersive fullscreen.
  immersive_revealed_lock_.reset();
  // Unregister from observing previous |overlay_web_contents_|.
  registrar_.RemoveAll();
}

void OverlayContainer::SetOverlay(views::WebView* overlay,
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

  bool repaint_infobars = false;

  if (overlay_ != overlay) {
    if (overlay_) {
      RemoveChildView(overlay_);
    } else {
      // There's no previous overlay, repaint infobars when showing the new one.
      repaint_infobars = true;
    }

    overlay_ = overlay;
    if (overlay_) {
      AddChildView(overlay_);
      // Hold the top views open in immersive fullscreen.
      immersive_revealed_lock_.reset(
          immersive_mode_controller_->GetRevealedLock(
              ImmersiveModeController::ANIMATE_REVEAL_NO));
    } else {
      // There's no more overlay, repaint infobars that were obscured by the
      // previous overlay.
      repaint_infobars = true;
      // Allow top views to close in immersive fullscreen.
      immersive_revealed_lock_.reset();
    }
  }

  if (overlay_web_contents_ != overlay_web_contents) {
    // Unregister from observing previous |overlay_web_contents_|.
    registrar_.RemoveAll();

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
    if (!shadow_view_.get()) {
      // Shadow view has not been created.
      shadow_view_.reset(new ShadowView());
      AddChildView(shadow_view_.get());
    }
#endif  // !defined(OS_WIN)
  } else if (!overlay_) {
    shadow_view_.reset();
  }

  // Force re-layout of parent and/or re-layout of infobars.
  browser_view_->OnOverlayStateChanged(repaint_infobars);
}

gfx::Rect OverlayContainer::GetOverlayBounds() const {
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

bool OverlayContainer::WillOverlayBeFullHeight(
    int overlay_height,
    InstantSizeUnits overlay_height_units) const {
  return IsFullHeight(overlay_height, overlay_height_units);
}

bool OverlayContainer::IsOverlayFullHeight() const {
  return overlay_ && IsFullHeight(overlay_height_, overlay_height_units_);
}

gfx::Size OverlayContainer::GetPreferredSize() {
  if (!overlay_)
    return gfx::Size();
  // Make sure IsOverlayFullHeight() is called before this function, because
  // when overlay is full height, its height is determined by its parent
  // (i.e. whatever is available from below toolbar), which is not yet known
  // now.
  DCHECK(!IsOverlayFullHeight());
  // Width doesn't matter.
  // Height includes overlap with toolbar, |overlay_|'s height and, if
  // necessary, |shadow_view_|'s height.
  gfx::Size preferred_size(0, kToolbarOverlap + overlay_height_);
  if (shadow_view_)
    preferred_size.Enlarge(0, shadow_view_->GetPreferredSize().height());
  return preferred_size;
}

void OverlayContainer::Layout() {
  if (!overlay_)
    return;
  int target_overlay_height = OverlayHeightInPixels(
      height() - kToolbarOverlap, overlay_height_, overlay_height_units_);
  overlay_->SetBounds(0, kToolbarOverlap, width(), target_overlay_height);
  if (draw_drop_shadow_) {
#if !defined(OS_WIN)
    DCHECK(shadow_view_.get() && shadow_view_->parent());
    shadow_view_->SetBounds(0, overlay_->bounds().bottom(), width(),
                            shadow_view_->GetPreferredSize().height());
#endif  // !defined(OS_WIN)
  }

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

void OverlayContainer::OnPaint(gfx::Canvas* canvas) {
  // Paint the region which overlaps with the toolbar as a continuation of the
  // toolbar, like attached bookmark bar does.
  gfx::Rect toolbar_background_bounds(width(),
      BookmarkBarView::kToolbarAttachedBookmarkBarOverlap);
  gfx::Point background_image_offset =
      browser_view_->OffsetPointForToolbarBackgroundImage(
          gfx::Point(GetMirroredX(), y()));
  DetachableToolbarView::PaintBackgroundAttachedMode(canvas, GetThemeProvider(),
      toolbar_background_bounds, background_image_offset,
      browser_view_->browser()->host_desktop_type());
  // Paint a separator below toolbar.
  canvas->FillRect(
      gfx::Rect(0,
                BookmarkBarView::kToolbarAttachedBookmarkBarOverlap,
                width(),
                views::NonClientFrameView::kClientEdgeThickness),
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_SEPARATOR));
}

const char* OverlayContainer::GetClassName() const {
  return "OverlayContainer";
}

void OverlayContainer::Observe(int type,
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
