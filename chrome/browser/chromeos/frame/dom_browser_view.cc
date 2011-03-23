// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/dom_browser_view.h"

#include "chrome/browser/chromeos/frame/dom_browser_view_layout.h"
#include "ui/gfx/rect.h"
#include "views/widget/widget.h"

namespace chromeos {

// DOMBrowserView, public ------------------------------------------------------

DOMBrowserView::DOMBrowserView(Browser* browser)
    : chromeos::BrowserView(browser) {}

DOMBrowserView::~DOMBrowserView() {}

// static
BrowserWindow* DOMBrowserView::CreateDOMWindow(Browser* browser) {
  DOMBrowserView* view = new DOMBrowserView(browser);
  BrowserFrame::Create(view, browser->profile());
  return view;
}

void DOMBrowserView::WindowMoveOrResizeStarted() {}

gfx::Rect DOMBrowserView::GetToolbarBounds() const {
  return gfx::Rect();
}

int DOMBrowserView::GetTabStripHeight() const {
  return 0;
}

bool DOMBrowserView::IsTabStripVisible() const {
  return false;
}

bool DOMBrowserView::AcceleratorPressed(const views::Accelerator& accelerator) {
  return false;
}

void DOMBrowserView::SetStarredState(bool is_starred) {}

LocationBar* DOMBrowserView::GetLocationBar() const {
  return NULL;
}

void DOMBrowserView::SetFocusToLocationBar(bool select_all) {}

void DOMBrowserView::UpdateReloadStopState(bool is_loading, bool force) {}

void DOMBrowserView::UpdateToolbar(TabContentsWrapper* contents,
                                bool should_restore_state) {}

void DOMBrowserView::FocusToolbar() {}

void DOMBrowserView::FocusAppMenu() {}

void DOMBrowserView::ShowBookmarkBubble(const GURL& url,
                                        bool already_bookmarked) {}

void DOMBrowserView::ShowAppMenu() {}

LocationBarView* DOMBrowserView::GetLocationBarView() const {
  return NULL;
}

ToolbarView* DOMBrowserView::GetToolbarView() const {
  return NULL;
}

bool DOMBrowserView::ShouldShowOffTheRecordAvatar() const {
  return false;
}

bool DOMBrowserView::GetAcceleratorForCommandId(int command_id,
                                                ui::Accelerator* accelerator) {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

bool DOMBrowserView::IsToolbarVisible() const {
  return false;
}

// DOMBrowserView, protected ---------------------------------------------------

void DOMBrowserView::GetAccessiblePanes(
    std::vector<AccessiblePaneView*>* panes) {}

void DOMBrowserView::PaintChildren(gfx::Canvas* canvas) {
  views::ClientView::PaintChildren(canvas);
}

void DOMBrowserView::InitTabStrip(TabStripModel* model) {}

views::LayoutManager* DOMBrowserView::CreateLayoutManager() const {
  return new DOMBrowserViewLayout;
}

ToolbarView* DOMBrowserView::CreateToolbar() const {
  return NULL;
}

void DOMBrowserView::LoadingAnimationCallback() {}

}  // namespace chromeos
