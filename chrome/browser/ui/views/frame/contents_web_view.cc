// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_web_view.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"

ContentsWebView::ContentsWebView(content::BrowserContext* browser_context)
    : views::WebView(browser_context),
      status_bubble_(NULL) {
}

ContentsWebView::~ContentsWebView() {
}

void ContentsWebView::SetStatusBubble(StatusBubbleViews* status_bubble) {
  status_bubble_ = status_bubble;
  DCHECK(!status_bubble_ || status_bubble_->base_view() == this);
  if (status_bubble_)
    status_bubble_->Reposition();
}

bool ContentsWebView::NeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void ContentsWebView::OnVisibleBoundsChanged() {
  if (status_bubble_)
    status_bubble_->Reposition();
}

void ContentsWebView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  WebView::ViewHierarchyChanged(details);
  if (details.is_add)
    OnThemeChanged();
}

void ContentsWebView::OnThemeChanged() {
  ui::ThemeProvider* const theme = GetThemeProvider();
  if (!theme)
    return;

  // Set the background color to a dark tint of the new tab page's background
  // color.  This is the color filled within the WebView's bounds when its child
  // view is sized specially for fullscreen tab capture.  See WebView header
  // file comments for more details.
  const int kBackgroundBrightness = 0x33;  // 20%
  const SkColor ntp_background =
      theme->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  set_background(views::Background::CreateSolidBackground(
      SkColorGetR(ntp_background) * kBackgroundBrightness / 0xFF,
      SkColorGetG(ntp_background) * kBackgroundBrightness / 0xFF,
      SkColorGetB(ntp_background) * kBackgroundBrightness / 0xFF,
      SkColorGetA(ntp_background)));
}
