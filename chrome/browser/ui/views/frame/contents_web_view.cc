// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_web_view.h"

#include "chrome/browser/ui/views/status_bubble_views.h"

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
