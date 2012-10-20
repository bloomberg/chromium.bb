// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search/search_ntp_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/ntp_background_util.h"
#include "chrome/browser/ui/search/search_ui.h"
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
                                               views::View* browser_view)
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

void SearchNTPContainerView::Layout() {
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

  // This is a hack to patch up ordering of native layer.  Changes to the view
  // hierarchy can |ReorderLayers| which messes with layer stacking.  Layout
  // typically follows reorderings, so we patch things up here.
  if (content_view_->web_contents()) {
    ui::Layer* native_view_layer =
        content_view_->web_contents()->GetNativeView()->layer();
    native_view_layer->parent()->StackAtTop(native_view_layer);
  }
}

void SearchNTPContainerView::OnPaintBackground(gfx::Canvas* canvas) {
  int paint_height = height();
  if (content_view_)
    paint_height = content_view_->bounds().y();

  gfx::Rect paint_rect(0, 0, width(), paint_height);
  gfx::Rect paint_rect_in_browser_view(paint_rect);
  gfx::Point origin;
  View::ConvertPointToTarget(this, browser_view_, &origin);
  paint_rect_in_browser_view.set_origin(origin);
  NtpBackgroundUtil::PaintBackgroundForBrowserClientArea(profile_, canvas,
      paint_rect, browser_view_->size(), paint_rect_in_browser_view);

  // Have to use the height of the layer here since the layer is animated
  // independent of the view.
  int height = layer()->bounds().height();
  if (height < chrome::search::kSearchResultsHeight)
    return;
  canvas->FillRect(gfx::Rect(0, height - 1, width(), 1),
                   chrome::search::kResultsSeparatorColor);
}

}  // namespace internal

