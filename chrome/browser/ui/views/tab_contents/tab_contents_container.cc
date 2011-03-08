// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"


////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, public:

TabContentsContainer::~TabContentsContainer() {
  if (tab_contents_)
    RemoveObservers();
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

AccessibilityTypes::Role TabContentsContainer::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_WINDOW;
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

void TabContentsContainer::TabContentsDestroyed(TabContents* contents) {
  // Sometimes, a TabContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  DCHECK(contents == tab_contents_);
  ChangeTabContents(NULL);
}

void TabContentsContainer::RenderWidgetHostViewChanged(
    RenderWidgetHostView* new_view) {
  if (new_view)
    new_view->set_reserved_contents_rect(cached_reserved_rect_);
}
