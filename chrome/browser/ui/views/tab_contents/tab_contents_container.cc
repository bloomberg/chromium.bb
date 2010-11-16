// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tab_contents/tab_contents_container.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/tab_contents/native_tab_contents_container.h"
#include "chrome/common/notification_service.h"

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, public:

TabContentsContainer::TabContentsContainer()
    : native_container_(NULL),
      tab_contents_(NULL),
      reserved_area_delegate_(NULL) {
  SetID(VIEW_ID_TAB_CONTAINER);
}

TabContentsContainer::~TabContentsContainer() {
  if (tab_contents_)
    RemoveObservers();
}

void TabContentsContainer::ChangeTabContents(TabContents* contents) {
  if (tab_contents_) {
    native_container_->DetachContents(tab_contents_);
    tab_contents_->WasHidden();
    RemoveObservers();
  }
  TabContents* old_contents = tab_contents_;
  tab_contents_ = contents;
  // When detaching the last tab of the browser ChangeTabContents is invoked
  // with NULL. Don't attempt to do anything in that case.
  if (tab_contents_) {
    RenderWidgetHostViewChanged(
        old_contents ? old_contents->GetRenderWidgetHostView() : NULL,
        tab_contents_->GetRenderWidgetHostView());
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
// TabContentsContainer, NotificationObserver implementation:

void TabContentsContainer::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::RENDER_VIEW_HOST_CHANGED) {
    RenderViewHostSwitchedDetails* switched_details =
        Details<RenderViewHostSwitchedDetails>(details).ptr();
    RenderViewHostChanged(switched_details->old_host,
                          switched_details->new_host);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    TabContentsDestroyed(Source<TabContents>(source).ptr());
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, View overrides:

void TabContentsContainer::Layout() {
  if (native_container_) {
    if (reserved_area_delegate_)
      reserved_area_delegate_->UpdateReservedContentsRect(this);
    native_container_->GetView()->SetBounds(0, 0, width(), height());
    native_container_->GetView()->Layout();
  }
}

AccessibilityTypes::Role TabContentsContainer::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_WINDOW;
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

void TabContentsContainer::AddObservers() {
  // TabContents can change their RenderViewHost and hence the HWND that is
  // shown and getting focused.  We need to keep track of that so we install
  // the focus subclass on the shown HWND so we intercept focus change events.
  registrar_.Add(this,
                 NotificationType::RENDER_VIEW_HOST_CHANGED,
                 Source<NavigationController>(&tab_contents_->controller()));

  registrar_.Add(this,
                 NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents_));
}

void TabContentsContainer::RemoveObservers() {
  registrar_.RemoveAll();
}

void TabContentsContainer::RenderViewHostChanged(RenderViewHost* old_host,
                                                 RenderViewHost* new_host) {
  if (new_host) {
    RenderWidgetHostViewChanged(
        old_host ? old_host->view() : NULL, new_host->view());
  }
  native_container_->RenderViewHostChanged(old_host, new_host);
}

void TabContentsContainer::TabContentsDestroyed(TabContents* contents) {
  // Sometimes, a TabContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  DCHECK(contents == tab_contents_);
  ChangeTabContents(NULL);
}

void TabContentsContainer::RenderWidgetHostViewChanged(
    RenderWidgetHostView* old_view, RenderWidgetHostView* new_view) {
  // Carry over the reserved rect, if possible.
  if (old_view && new_view) {
    new_view->set_reserved_contents_rect(old_view->reserved_contents_rect());
  } else {
    if (reserved_area_delegate_)
      reserved_area_delegate_->UpdateReservedContentsRect(this);
  }
}
