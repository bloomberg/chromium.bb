// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search/search_ntp_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/ntp_background_util.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/search/search_view_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/webview/webview.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace internal {

SearchNTPContainerView::SearchNTPContainerView(Profile* profile,
                                               BrowserView* browser_view)
    : profile_(profile),
      browser_view_(browser_view),
      logo_view_(NULL),
      content_view_(NULL),
      notified_css_background_y_pos_(0),
      to_notify_css_background_y_pos_(false) {
}

SearchNTPContainerView::~SearchNTPContainerView() {
}

void SearchNTPContainerView::SetLogoView(views::View* logo_view) {
  if (logo_view_ == logo_view)
    return;
  if (logo_view_)
    RemoveChildView(logo_view_);
  logo_view_ = logo_view;
  if (logo_view_)
    AddChildView(logo_view_);
}

void SearchNTPContainerView::SetContentView(views::WebView* content_view) {
  if (content_view_ == content_view)
    return;
  if (content_view_)
    RemoveChildView(content_view_);
  content_view_ = content_view;
  if (content_view_)
    AddChildView(content_view_);
}

void SearchNTPContainerView::NotifyNTPBackgroundYPosIfChanged(
    views::WebView* content_view, const chrome::search::Mode& mode) {
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  bool has_theme = tp->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  int alignment;
  if (has_theme)
    tp->GetDisplayProperty(ThemeService::NTP_BACKGROUND_ALIGNMENT, &alignment);

  // Determine |background-position| of new_tab_theme.css for NTP page according
  // to vertical alignment of theme images:
  // - top-aligned: y-pos of |content_view| relative to its parent
  //   (i.e. SearchNTPContainerView), because NTP background theme is only used
  //   for SearchNTPContainerView, i.e. without tab and toolbar.
  // - vertically-centered: formula based on y-pos and bounds of |content_view|
  //   relative to |BrowserView|, because NTP background is used for entire
  //   |BrowserView| from top of tab to bottom of browser view.
  // - bottom-aligned: always 0, because bottom-left of |content_view| is the
  //   origin of the image.
  // Non-NTP pages always use 0 for |background-position|.
  int new_css_background_y_pos = 0;
  if (has_theme && mode.is_ntp()) {
    if (alignment & ThemeService::ALIGN_TOP) {
      new_css_background_y_pos = content_view->bounds().y();
    } else if (alignment & ThemeService::ALIGN_BOTTOM) {
        new_css_background_y_pos = 0;
    } else { // ALIGN_CENTER
      // Convert new bounds of |content_view| relative to |BrowserView|.
      gfx::Point origin(content_view->bounds().origin());
      views::View::ConvertPointToTarget(content_view->parent(), browser_view_,
                                        &origin);
      // Determine y-pos of center-aligned image.
      gfx::ImageSkia* ntp_background =
          tp->GetImageSkiaNamed(IDR_THEME_NTP_BACKGROUND);
      int image_y = NtpBackgroundUtil::GetPlacementOfCenterAlignedImage(
          browser_view_->height(), ntp_background->height());
      new_css_background_y_pos = origin.y() - image_y;
    }
  }

  if (!to_notify_css_background_y_pos_ &&
      new_css_background_y_pos == notified_css_background_y_pos_)
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_NTP_BACKGROUND_THEME_Y_POS_CHANGED,
      content::Source<Profile>(profile_),
      content::Details<int>(&new_css_background_y_pos));
  notified_css_background_y_pos_ = new_css_background_y_pos;
  to_notify_css_background_y_pos_ = false;
}

void SearchNTPContainerView::MaybeStackBookmarkBarAtTop() {
  BookmarkBarView* bookmark_bar = browser_view_->bookmark_bar();
  if (bookmark_bar)
    bookmark_bar->MaybeStackAtTop();
}

void SearchNTPContainerView::Layout() {
  // Layout logo view.
  if (!logo_view_)
    return;
  gfx::Size preferred_size = logo_view_->GetPreferredSize();
  logo_view_->SetBounds(
      (width() - preferred_size.width()) / 2,
      chrome::search::kLogoYPosition,
      preferred_size.width(),
      preferred_size.height());

  // Note: Next would be the Omnibox layout.  That is done in
  // |ToolbarView::LayoutForSearch| however.

  // Layout content view.
  if (!content_view_)
    return;
  const int kContentTop = logo_view_->bounds().bottom() +
      chrome::search::kLogoBottomGap +
      chrome::search::kNTPOmniboxHeight +
      chrome::search::kOmniboxBottomGap;
  gfx::Rect old_content_bounds(content_view_->bounds());
  content_view_->SetBounds(0, kContentTop, width(), height() - kContentTop);
  NotifyNTPBackgroundYPosIfChanged(content_view_,
      chrome::search::Mode(chrome::search::Mode::MODE_NTP, false));
#if defined(USE_AURA)
  // This is a hack to patch up ordering of native layer.  Changes to the view
  // hierarchy can |ReorderLayers| which messes with layer stacking.  Layout
  // typically follows reorderings, so we patch things up here.
  if (content_view_->web_contents()) {
    ui::Layer* native_view_layer =
        content_view_->web_contents()->GetNativeView()->layer();
    native_view_layer->parent()->StackAtTop(native_view_layer);
  }
#endif  // USE_AURA

  // Layout bookmark bar, so that it floats on top of content view (in z-order)
  // and below the "Most visited" thumbnails (in the y-direction) at the bottom
  // of content view.
  BookmarkBarView* bookmark_bar = browser_view_->bookmark_bar();
  if (!bookmark_bar)
    return;
  // Only show bookmark bar if height of content view is >=
  // chrome::search::kMinContentHeightForBottomBookmarkBar.
  // Bookmark bar is child of |BrowserView|, so convert coordinates relative
  // to |BrowserView|.
  gfx::Rect content_bounds(content_view_->bounds());
  gfx::Point content_origin(content_bounds.origin());
  views::View::ConvertPointToTarget(this, browser_view_, &content_origin);
  content_bounds.set_origin(content_origin);
  if (content_bounds.height() <
          chrome::search::kMinContentHeightForBottomBookmarkBar ||
      !browser_view_->IsBookmarkBarVisible()) {
    bookmark_bar->SetVisible(false);
    bookmark_bar->SetBounds(0, content_bounds.bottom(),
                            content_bounds.width(), 0);
    return;
  }
  // BookmarkBarView uses infobar visibility to determine toolbar overlap, which
  // is 0 if bookmark bar is detached and infobar is visible.  Since NTP
  // bookmark bar is detached at bottom of content view, toolbar overlap is
  // irrelevant.  So set infobar visible to force 0 toolbar overlap.
  bookmark_bar->set_infobar_visible(true);
  bookmark_bar->SetVisible(true);
  int omnibox_width = 0;
  int omnibox_xpos = 0;
  browser_view_->search_view_controller()->GetNTPOmniboxWidthAndXPos(
      content_bounds.width(), &omnibox_width, &omnibox_xpos);
  int bookmark_bar_height = bookmark_bar->GetPreferredSize().height();
  bookmark_bar->SetBounds(omnibox_xpos,
                          content_bounds.bottom() - bookmark_bar_height,
                          omnibox_width,
                          bookmark_bar_height);

  // Detached bottom bookmark bar should always go on top.
  MaybeStackBookmarkBarAtTop();
}

void SearchNTPContainerView::OnPaintBackground(gfx::Canvas* canvas) {
  // While it's not necessary to paint background of content view here (since
  // content view does so itself), content view shows up later than the logo and
  // omnibox, resulting in a black flash in the content view before it paints.
  // So, paint the entire background here.
  gfx::Rect paint_rect(0, 0, width(), height());
  gfx::Rect paint_rect_in_browser_view(paint_rect);
  gfx::Point origin;
  View::ConvertPointToTarget(this, browser_view_, &origin);
  paint_rect_in_browser_view.set_origin(origin);
  NtpBackgroundUtil::PaintBackgroundForBrowserClientArea(profile_, canvas,
      paint_rect, browser_view_->size(), paint_rect_in_browser_view);

#if defined(USE_AURA)
  // Have to use the height of the layer here since the layer is animated
  // independent of the view.
  int height = layer()->bounds().height();
#else
  // When we don't use Aura, we don't animate, so the height of the view is ok.
  int height = bounds().height();
#endif
  if (height < chrome::search::kSearchResultsHeight)
    return;
  canvas->FillRect(gfx::Rect(0, height - 1, width(), 1),
                   chrome::search::kResultsSeparatorColor);
}

}  // namespace internal

