// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dom_view.h"

#include "content/browser/tab_contents/tab_contents.h"
#include "views/focus/focus_manager.h"

#if defined(TOUCH_UI)
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#endif

DOMView::DOMView() : tab_contents_(NULL), initialized_(false) {
  SetFocusable(true);
}

DOMView::~DOMView() {
  if (native_view())
    Detach();
}

bool DOMView::Init(Profile* profile, SiteInstance* instance) {
  if (initialized_)
    return true;

  initialized_ = true;
  tab_contents_.reset(CreateTabContents(profile, instance));
  // Attach the native_view now if the view is already added to Widget.
  if (GetWidget())
    AttachTabContents();

  return true;
}

TabContents* DOMView::CreateTabContents(Profile* profile,
                                        SiteInstance* instance) {
  return new TabContents(profile, instance, MSG_ROUTING_NONE, NULL, NULL);
}

void DOMView::LoadURL(const GURL& url) {
  DCHECK(initialized_);
  tab_contents_->controller().LoadURL(url, GURL(), PageTransition::START_PAGE);
}

bool DOMView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  // Don't move the focus to the next view when tab is pressed, we want the
  // key event to be propagated to the render view for doing the tab traversal
  // there.
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

void DOMView::OnFocus() {
  tab_contents_->Focus();
}

void DOMView::ViewHierarchyChanged(bool is_add, views::View* parent,
                                   views::View* child) {
  // Attach the native_view when this is added to Widget if
  // the native view has not been attached yet and tab_contents_ exists.
  views::NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !native_view() && tab_contents_.get())
    AttachTabContents();
  else if (!is_add && child == this && native_view())
    Detach();
}

void DOMView::AttachTabContents() {
#if defined(TOUCH_UI)
  AttachToView(static_cast<TabContentsViewTouch*>(tab_contents_->view()));
#else
  Attach(tab_contents_->GetNativeView());
#endif
}
