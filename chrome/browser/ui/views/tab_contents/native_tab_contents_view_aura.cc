// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_aura.h"

#include "chrome/browser/tab_contents/web_drop_target_win.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_views.h"
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "views/views_delegate.h"
#include "views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, public:

NativeTabContentsViewAura::NativeTabContentsViewAura(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::NativeWidgetAura(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

NativeTabContentsViewAura::~NativeTabContentsViewAura() {
}

TabContents* NativeTabContentsViewAura::GetTabContents() const {
  return delegate_->GetTabContents();
}

void NativeTabContentsViewAura::EndDragging() {
  delegate_->OnNativeTabContentsViewDraggingEnded();
  // TODO(beng):
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, NativeTabContentsView implementation:

void NativeTabContentsViewAura::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = NULL;
  GetWidget()->Init(params);
}

void NativeTabContentsViewAura::Unparent() {
  // Note that we do not DCHECK on focus_manager_ as it may be NULL when used
  // with an external tab container.
  views::Widget::ReparentNativeView(GetNativeView(), NULL);
  // TODO(beng):
  NOTIMPLEMENTED();
}

RenderWidgetHostView* NativeTabContentsViewAura::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  // TODO(beng): probably return RenderWidgetHostViewViews.
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeWindow NativeTabContentsViewAura::GetTopLevelNativeWindow() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return NULL;
}

void NativeTabContentsViewAura::SetPageTitle(const string16& title) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void NativeTabContentsViewAura::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void NativeTabContentsViewAura::CancelDrag() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

bool NativeTabContentsViewAura::IsDoingDrag() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return false;
}

void NativeTabContentsViewAura::SetDragCursor(
    WebKit::WebDragOperation operation) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

views::NativeWidget* NativeTabContentsViewAura::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, views::NativeWidgetWin overrides:

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewViews(delegate);
  // return new NativeTabContentsViewAura(delegate);
}
