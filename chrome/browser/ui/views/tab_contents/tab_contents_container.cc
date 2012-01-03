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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/accessibility/accessible_view_state.h"

using content::WebContents;

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, public:

TabContentsContainer::TabContentsContainer()
    : native_container_(NULL),
      web_contents_(NULL) {
  set_id(VIEW_ID_TAB_CONTAINER);
}

TabContentsContainer::~TabContentsContainer() {
  if (web_contents_)
    RemoveObservers();
}

void TabContentsContainer::ChangeWebContents(WebContents* contents) {
  if (web_contents_) {
    native_container_->DetachContents(web_contents_);
    web_contents_->WasHidden();
    RemoveObservers();
  }
  web_contents_ = contents;
  // When detaching the last tab of the browser ChangeWebContents is invoked
  // with NULL. Don't attempt to do anything in that case.
  if (web_contents_) {
    RenderWidgetHostViewChanged(web_contents_->GetRenderWidgetHostView());
    native_container_->AttachContents(web_contents_);
    AddObservers();
  }
}

content::WebContents* TabContentsContainer::web_contents() {
  return web_contents_;
}

void TabContentsContainer::WebContentsFocused(WebContents* contents) {
  native_container_->WebContentsFocused(contents);
}

void TabContentsContainer::SetFastResize(bool fast_resize) {
  native_container_->SetFastResize(fast_resize);
}

void TabContentsContainer::SetReservedContentsRect(
    const gfx::Rect& reserved_rect) {
  cached_reserved_rect_ = reserved_rect;
  if (web_contents_ && web_contents_->GetRenderWidgetHostView()) {
    web_contents_->GetRenderWidgetHostView()->set_reserved_contents_rect(
        reserved_rect);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, content::NotificationObserver implementation:

void TabContentsContainer::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    RenderViewHostSwitchedDetails* switched_details =
        content::Details<RenderViewHostSwitchedDetails>(details).ptr();
    RenderViewHostChanged(switched_details->old_host,
                          switched_details->new_host);
  } else if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    TabContentsDestroyed(content::Source<WebContents>(source).ptr());
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsContainer, View overrides:

void TabContentsContainer::Layout() {
  if (native_container_) {
    gfx::Size view_size(native_container_->GetView()->size());
    native_container_->GetView()->SetBounds(0, 0, width(), height());
    // SetBounds does nothing if the bounds haven't changed. We need to force
    // layout if the bounds haven't changed, but fast resize has.
    if (view_size.width() == width() && view_size.height() == height() &&
        native_container_->FastResizeAtLastLayout() &&
        !native_container_->GetFastResize()) {
      native_container_->GetView()->Layout();
    }
  }
}

void TabContentsContainer::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_WINDOW;
}

#if defined(HAVE_XINPUT2)
bool TabContentsContainer::OnMousePressed(const views::MouseEvent& event) {
  DCHECK(web_contents_);
  if (event.flags() & (ui::EF_LEFT_MOUSE_BUTTON |
                       ui::EF_RIGHT_MOUSE_BUTTON |
                       ui::EF_MIDDLE_MOUSE_BUTTON)) {
    return false;
  }
  // It is necessary to look at the native event to determine what special
  // button was pressed.
  views::NativeEvent native_event = event.native_event();
  if (!native_event)
    return false;

  int button = 0;
  switch (native_event->type) {
    case ButtonPress: {
      button = native_event->xbutton.button;
      break;
    }
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      button = xievent->detail;
      break;
    }
    default:
      break;
  }
  switch (button) {
    case 8:
      web_contents_->GetController().GoBack();
      return true;
    case 9:
      web_contents_->GetController().GoForward();
      return true;
  }

  return false;
}
#endif

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
  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      content::Source<content::NavigationController>(
          &web_contents_->GetController()));

  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(web_contents_));
}

void TabContentsContainer::RemoveObservers() {
  registrar_.RemoveAll();
}

void TabContentsContainer::RenderViewHostChanged(RenderViewHost* old_host,
                                                 RenderViewHost* new_host) {
  if (new_host)
    RenderWidgetHostViewChanged(new_host->view());
  native_container_->RenderViewHostChanged(old_host, new_host);
}

void TabContentsContainer::TabContentsDestroyed(WebContents* contents) {
  // Sometimes, a TabContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  DCHECK(contents == web_contents_);
  ChangeWebContents(NULL);
}

void TabContentsContainer::RenderWidgetHostViewChanged(
    RenderWidgetHostView* new_view) {
  if (new_view)
    new_view->set_reserved_contents_rect(cached_reserved_rect_);
}
