// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container_gtk.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "views/layout/fill_layout.h"

// Some of this class is implemented in tab_contents_container.cc, where
// the implementation doesn't vary between a pure views approach and a
// native view host approach. See the header file for details.

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, public:

TabContentsContainer::TabContentsContainer()
    : tab_contents_(NULL) {
  SetID(VIEW_ID_TAB_CONTAINER);
}

void TabContentsContainer::SetReservedContentsRect(
    const gfx::Rect& reserved_rect) {
  cached_reserved_rect_ = reserved_rect;
  // TODO(anicolao): find out what this is supposed to be used for and ensure
  // it's OK for touch.
}

void TabContentsContainer::ChangeTabContents(TabContents* contents) {
  if (tab_contents_) {
    views::View *v = static_cast<TabContentsViewTouch*>(tab_contents_->view());
    RemoveChildView(v);
    tab_contents_->WasHidden();
    RemoveObservers();
  }
  tab_contents_ = contents;
  // When detaching the last tab of the browser ChangeTabContents is invoked
  // with NULL. Don't attempt to do anything in that case.
  if (tab_contents_) {
    views::View *v = static_cast<TabContentsViewTouch*>(contents->view());
    AddChildView(v);
    SetLayoutManager(new views::FillLayout());
    Layout();
    AddObservers();
  }
}

void TabContentsContainer::TabContentsFocused(TabContents* tab_contents) {
}

void TabContentsContainer::SetFastResize(bool fast_resize) {
}

void TabContentsContainer::RenderViewHostChanged(RenderViewHost* old_host,
                                                 RenderViewHost* new_host) {
  NOTIMPLEMENTED();  // TODO(anicolao)
}
