// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "content/public/browser/notification_source.h"
#include "ui/gfx/geometry/rect.h"

LocationBarBubbleDelegateView::LocationBarBubbleDelegateView(
    views::View* anchor_view,
    content::WebContents* web_contents)
    : BubbleDelegateView(anchor_view,
                         anchor_view ? views::BubbleBorder::TOP_RIGHT
                                     : views::BubbleBorder::NONE) {
  // Add observer to close the bubble if the fullscreen state changes.
  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    registrar_.Add(
        this, chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::Source<FullscreenController>(
            browser->exclusive_access_manager()->fullscreen_controller()));
  }
}

LocationBarBubbleDelegateView::~LocationBarBubbleDelegateView() {}

void LocationBarBubbleDelegateView::ShowForReason(DisplayReason reason) {
  if (reason == USER_GESTURE) {
    SetArrowPaintType(views::BubbleBorder::PAINT_TRANSPARENT);
    GetWidget()->Show();
  } else {
    GetWidget()->ShowInactive();
  }
}

void LocationBarBubbleDelegateView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  GetWidget()->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  Close();
}

void LocationBarBubbleDelegateView::Close() {
  views::Widget* widget = GetWidget();
  if (!widget->IsClosed())
    widget->Close();
}

void LocationBarBubbleDelegateView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  const int kBubblePaddingFromScreenEdge = 20;
  int horizontal_offset = width() / 2 + kBubblePaddingFromScreenEdge;
  const int x_pos = base::i18n::IsRTL()
                        ? (screen_bounds.x() + horizontal_offset)
                        : (screen_bounds.right() - horizontal_offset);
  SetAnchorRect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
}
