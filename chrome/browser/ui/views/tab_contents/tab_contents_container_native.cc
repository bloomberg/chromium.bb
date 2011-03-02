// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"

// Some of this class is implemented in tab_contents_container.cc, where
// the implementation doesn't vary between a pure views approach and a
// native view host approach. See the header file for details.

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, public:

TabContentsContainer::TabContentsContainer()
    : native_container_(NULL),
      tab_contents_(NULL) {
  SetID(VIEW_ID_TAB_CONTAINER);
}

void TabContentsContainer::SetReservedContentsRect(
    const gfx::Rect& reserved_rect) {
  cached_reserved_rect_ = reserved_rect;
  if (tab_contents_ && tab_contents_->GetRenderWidgetHostView()) {
    tab_contents_->GetRenderWidgetHostView()->set_reserved_contents_rect(
        reserved_rect);
  }
}

void TabContentsContainer::ChangeTabContents(TabContents* contents) {
  if (tab_contents_) {
    native_container_->DetachContents(tab_contents_);
    tab_contents_->WasHidden();
    RemoveObservers();
  }
  tab_contents_ = contents;
  // When detaching the last tab of the browser ChangeTabContents is invoked
  // with NULL. Don't attempt to do anything in that case.
  if (tab_contents_) {
    RenderWidgetHostViewChanged(tab_contents_->GetRenderWidgetHostView());
    native_container_->AttachContents(tab_contents_);
    AddObservers();
  }
}

void TabContentsContainer::TabContentsFocused(TabContents* tab_contents) {
  native_container_->TabContentsFocused(tab_contents);
}

void TabContentsContainer::SetFastResize(bool fast_resize) {
  native_container_->SetFastResize(fast_resize);
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, View overrides:

void TabContentsContainer::Layout() {
  if (native_container_) {
    native_container_->GetView()->SetBounds(0, 0, width(), height());
    native_container_->GetView()->Layout();
  }
}

void TabContentsContainer::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this) {
    native_container_ = NativeTabContentsContainer::CreateNativeContainer(this);
    AddChildView(native_container_->GetView());
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, private:

void TabContentsContainer::RenderViewHostChanged(RenderViewHost* old_host,
                                                 RenderViewHost* new_host) {
  if (new_host)
    RenderWidgetHostViewChanged(new_host->view());
  native_container_->RenderViewHostChanged(old_host, new_host);
}
