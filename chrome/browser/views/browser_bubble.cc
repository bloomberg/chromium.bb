// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "app/l10n_util.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

namespace {

BrowserView* GetBrowserViewFromFrame(views::Widget* frame) {
  BrowserView* browser_view = NULL;
  views::Window* window = frame->GetWindow();
  if (window) {
    browser_view = BrowserView::GetBrowserViewForNativeWindow(
        window->GetNativeWindow());
    DCHECK(browser_view);
  }
  return browser_view;
}

}  // namespace

BrowserBubble::BrowserBubble(views::View* view, views::Widget* frame,
                             const gfx::Point& origin)
    : frame_(frame),
      view_(view),
      visible_(false),
      delegate_(NULL),
      attached_(false) {
  gfx::Size size = view->GetPreferredSize();
  bounds_.SetRect(origin.x(), origin.y(), size.width(), size.height());
  InitPopup();
}

BrowserBubble::~BrowserBubble() {
  DCHECK(!attached_);
  popup_->Close();

  // Don't call DetachFromBrowser from here.  It needs to talk to the
  // BrowserView to deregister itself, and if BrowserBubble is owned
  // by a child of BrowserView, then it's possible that this stack frame
  // is a descendant of BrowserView's destructor, which leads to problems.
  // In that case, Detach doesn't need to get called anyway since BrowserView
  // will do the necessary cleanup.
}

void BrowserBubble::DetachFromBrowser() {
  DCHECK(attached_);
  if (!attached_)
    return;
  attached_ = false;

  BrowserView* browser_view = GetBrowserViewFromFrame(frame_);
  if (browser_view)
    browser_view->DetachBrowserBubble(this);
}

void BrowserBubble::AttachToBrowser() {
  DCHECK(!attached_);
  if (attached_)
    return;

  BrowserView* browser_view = GetBrowserViewFromFrame(frame_);
  if (browser_view)
    browser_view->AttachBrowserBubble(this);

  attached_ = true;
}

void BrowserBubble::BrowserWindowMoved() {
  if (delegate_)
    delegate_->BubbleBrowserWindowMoved(this);
  else
    Hide();
  if (visible_)
    Reposition();
}

void BrowserBubble::BrowserWindowClosing() {
  if (delegate_)
    delegate_->BubbleBrowserWindowClosing(this);
  else
    Hide();
}

void BrowserBubble::SetBounds(int x, int y, int w, int h) {
  // If the UI layout is RTL, we don't need to mirror coordinates, since
  // View logic will do that for us.
  bounds_.SetRect(x, y, w, h);
  Reposition();
}

void BrowserBubble::MoveTo(int x, int y) {
  SetBounds(x, y, bounds_.width(), bounds_.height());
}

void BrowserBubble::Reposition() {
  gfx::Point top_left;
  views::View::ConvertPointToScreen(frame_->GetRootView(), &top_left);
  MovePopup(top_left.x() + bounds_.x(),
            top_left.y() + bounds_.y(),
            bounds_.width(),
            bounds_.height());
}

void BrowserBubble::ResizeToView() {
  SetBounds(bounds_.x(), bounds_.y(), view_->width(), view_->height());
}
