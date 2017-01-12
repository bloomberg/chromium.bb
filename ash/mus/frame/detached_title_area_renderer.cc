// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/frame/detached_title_area_renderer.h"

#include "ash/common/frame/header_view.h"
#include "ash/mus/frame/detached_title_area_renderer_host.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace mus {

DetachedTitleAreaRenderer::DetachedTitleAreaRenderer(
    DetachedTitleAreaRendererHost* host,
    views::Widget* frame,
    const gfx::Rect& bounds,
    Source source)
    : host_(host), frame_(frame), widget_(new views::Widget) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.delegate = this;
  params.name = "DetachedTitleAreaRenderer";
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  views::NativeWidgetAura* native_widget =
      new views::NativeWidgetAura(widget_, true);
  params.native_widget = native_widget;
  // TODO: making the reveal window a sibling is likely problematic.
  // http://crbug.com/640392, see also comment in GetVisibleBoundsInScreen().
  params.parent = frame->GetNativeView()->parent();
  params.bounds = bounds;
  widget_->Init(params);
  params.parent->StackChildAbove(widget_->GetNativeView(),
                                 frame->GetNativeView());
  HeaderView* header_view = new HeaderView(frame_);
  if (source == Source::CLIENT) {
    // HeaderView behaves differently when the widget it is associated with is
    // fullscreen (HeaderView is normally the
    // ImmersiveFullscreenControllerDelegate). Set this as when creating for
    // the client HeaderView is not the ImmersiveFullscreenControllerDelegate.
    header_view->set_is_immersive_delegate(false);
  }
  widget_->SetContentsView(header_view);
  widget_->GetRootView()->SetSize(bounds.size());
  widget_->ShowInactive();
}

void DetachedTitleAreaRenderer::Destroy() {
  host_ = nullptr;
  if (widget_)
    widget_->CloseNow();
}

views::Widget* DetachedTitleAreaRenderer::GetWidget() {
  return widget_;
}

const views::Widget* DetachedTitleAreaRenderer::GetWidget() const {
  return widget_;
}

void DetachedTitleAreaRenderer::DeleteDelegate() {
  if (host_)
    host_->OnDetachedTitleAreaRendererDestroyed(this);
  delete this;
}

DetachedTitleAreaRenderer::~DetachedTitleAreaRenderer() {}

}  // namespace mus
}  // namespace ash
