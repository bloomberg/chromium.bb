// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_bubble.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#if defined(OS_WIN)
#include "chrome/browser/external_tab_container_win.h"
#endif
#include "views/widget/root_view.h"
#include "views/window/window.h"

namespace {

BrowserBubbleHost* GetBubbleHostFromFrame(views::Widget* frame) {
  if (!frame)
    return NULL;

  BrowserBubbleHost* bubble_host = NULL;
  views::Window* window = frame->GetWindow();
  if (window) {
    bubble_host = BrowserView::GetBrowserViewForNativeWindow(
        window->GetNativeWindow());
    DCHECK(bubble_host);
  }

  return bubble_host;
}

}  // namespace

BrowserBubble::BrowserBubble(views::View* view, views::Widget* frame,
                             const gfx::Point& origin)
    : frame_(frame),
      view_(view),
      visible_(false),
      delegate_(NULL),
      attached_(false),
      bubble_host_(GetBubbleHostFromFrame(frame)) {
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

  if (bubble_host_)
    bubble_host_->DetachBrowserBubble(this);
}

void BrowserBubble::AttachToBrowser() {
  DCHECK(!attached_);
  if (attached_)
    return;

  if (bubble_host_)
    bubble_host_->AttachBrowserBubble(this);

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
