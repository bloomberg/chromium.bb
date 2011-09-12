// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_views.h"

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "views/widget/widget.h"

// TODO(beng): HiddenTabHostWindow??

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewViews, public:

NativeTabContentsViewViews::NativeTabContentsViewViews(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::NativeWidgetViews(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

NativeTabContentsViewViews::~NativeTabContentsViewViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewViews, NativeWidgetViews implementation:
void NativeTabContentsViewViews::OnBoundsChanged(
    const gfx::Rect& new_bounds, const gfx::Rect& old_bounds) {
  // TODO(oshima): Find out if we need to adjust constrained window.
  delegate_->OnNativeTabContentsViewSized(new_bounds.size());
  views::NativeWidgetViews::OnBoundsChanged(new_bounds, old_bounds);
}

bool NativeTabContentsViewViews::OnMouseEvent(const views::MouseEvent& event) {
  if (!delegate_->IsShowingSadTab()) {
    switch (event.type()) {
      case ui::ET_MOUSE_EXITED:
        delegate_->OnNativeTabContentsViewMouseMove(false);
        break;
      case ui::ET_MOUSE_MOVED:
        delegate_->OnNativeTabContentsViewMouseMove(true);
        break;
      default:
        // TODO(oshima): mouse wheel
        break;
    }
  }
  // Pass all mouse event to renderer.
  return views::NativeWidgetViews::OnMouseEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewViews, NativeTabContentsView implementation:

void NativeTabContentsViewViews::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  GetWidget()->Init(params);
}

void NativeTabContentsViewViews::Unparent() {
}

RenderWidgetHostView* NativeTabContentsViewViews::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  // Remove the old RenderWidgetHostView, otherwise SetContentsView will delete
  // it.
  views::View* old_rwhv = GetWidget()->GetContentsView();
  if (old_rwhv)
    old_rwhv->parent()->RemoveChildView(old_rwhv);

  RenderWidgetHostViewViews* view =
      new RenderWidgetHostViewViews(render_widget_host);
  GetWidget()->SetContentsView(view);
  view->Show();
  view->InitAsChild();
  // TODO(anicolao): implement drag'n'drop hooks if needed
  return view;
}

gfx::NativeWindow NativeTabContentsViewViews::GetTopLevelNativeWindow() const {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeTabContentsViewViews::SetPageTitle(const std::wstring& title) {
  SetWindowTitle(title);
}

void NativeTabContentsViewViews::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask ops,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  NOTIMPLEMENTED();
}

void NativeTabContentsViewViews::CancelDrag() {
  NOTIMPLEMENTED();
}

bool NativeTabContentsViewViews::IsDoingDrag() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeTabContentsViewViews::SetDragCursor(
    WebKit::WebDragOperation operation) {
  NOTIMPLEMENTED();
}

views::NativeWidget* NativeTabContentsViewViews::AsNativeWidget() {
  return this;
}

#if defined(TOUCH_UI)
// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewViews(delegate);
}
#endif
