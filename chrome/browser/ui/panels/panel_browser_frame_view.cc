// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_frame_view.h"

#include "chrome/browser/ui/panels/panel_browser_view.h"

PanelBrowserFrameView::PanelBrowserFrameView(BrowserFrame* frame,
                                             PanelBrowserView* browser_view)
    : OpaqueBrowserFrameView(frame, browser_view),
      browser_view_(browser_view) {
}

PanelBrowserFrameView::~PanelBrowserFrameView() {
}

bool PanelBrowserFrameView::OnMousePressed(const views::MouseEvent& event) {
  if (browser_view_->OnTitleBarMousePressed(event))
    return true;
  return OpaqueBrowserFrameView::OnMousePressed(event);
}

bool PanelBrowserFrameView::OnMouseDragged(const views::MouseEvent& event) {
  if (browser_view_->OnTitleBarMouseDragged(event))
    return true;
  return OpaqueBrowserFrameView::OnMouseDragged(event);
}

void PanelBrowserFrameView::OnMouseReleased(const views::MouseEvent& event) {
  if (browser_view_->OnTitleBarMouseReleased(event))
    return;
  OpaqueBrowserFrameView::OnMouseReleased(event);
}
