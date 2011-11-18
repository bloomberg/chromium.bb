// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dom_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/views/focus/focus_manager.h"
#include "views/widget/native_widget_views.h"

// static
const char DOMView::kViewClassName[] =
    "browser/ui/views/DOMView";

DOMView::DOMView() : initialized_(false) {
  set_focusable(true);
}

DOMView::~DOMView() {
  if (native_view())
    Detach();
}

std::string DOMView::GetClassName() const {
  return kViewClassName;
}

bool DOMView::Init(Profile* profile, SiteInstance* instance) {
  if (initialized_)
    return true;

  initialized_ = true;
  TabContents* tab_contents = CreateTabContents(profile, instance);
  dom_contents_.reset(new TabContentsWrapper(tab_contents));

  renderer_preferences_util::UpdateFromSystemSettings(
        tab_contents->GetMutableRendererPrefs(), profile);

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
  dom_contents_->tab_contents()->controller().LoadURL(
      url, GURL(), content::PAGE_TRANSITION_START_PAGE, std::string());
}

bool DOMView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  // Don't move the focus to the next view when tab is pressed, we want the
  // key event to be propagated to the render view for doing the tab traversal
  // there.
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

void DOMView::OnFocus() {
  dom_contents_->tab_contents()->Focus();
}

void DOMView::ViewHierarchyChanged(bool is_add, views::View* parent,
                                   views::View* child) {
  // Attach the native_view when this is added to Widget if
  // the native view has not been attached yet and tab_contents_ exists.
  views::NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !native_view() && dom_contents_.get())
    AttachTabContents();
  else if (!is_add && child == this && native_view())
    Detach();
}

void DOMView::AttachTabContents() {
#if !defined(USE_AURA)
  if (views::Widget::IsPureViews()) {
    TabContentsViewViews* widget = static_cast<TabContentsViewViews*>(
        dom_contents_->tab_contents()->view());
    views::NativeWidgetViews* nwv =
        static_cast<views::NativeWidgetViews*>(widget->native_widget());
    AttachToView(nwv->GetView());
  } else {
#endif
    Attach(dom_contents_->tab_contents()->GetNativeView());
#if !defined(USE_AURA)
  }
#endif
}
