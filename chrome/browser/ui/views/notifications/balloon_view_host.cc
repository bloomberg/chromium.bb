// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/notifications/balloon_view_host.h"

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#if defined(OS_WIN)
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#endif
#if defined(OS_LINUX)
#if defined(TOUCH_UI)
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#else
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#endif
#endif
#include "views/widget/widget.h"
#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#endif
#if defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

class BalloonViewHostView : public views::NativeViewHost {
 public:
  explicit BalloonViewHostView(BalloonViewHost* host)
      : host_(host),
        initialized_(false) {
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) {
    NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
    if (is_add && GetWidget() && !initialized_) {
      initialized_ = true;
      host_->Init(GetWidget()->GetNativeView());
    }
  }

 private:
  // The host owns this object.
  BalloonViewHost* host_;

  bool initialized_;
};

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : BalloonHost(balloon) {
  native_host_ = new BalloonViewHostView(this);
}

void BalloonViewHost::Init(gfx::NativeView parent_native_view) {
  parent_native_view_ = parent_native_view;
  BalloonHost::Init();
}

void BalloonViewHost::InitRenderWidgetHostView() {
  DCHECK(render_view_host_);

  render_widget_host_view_ =
      RenderWidgetHostView::CreateViewForWidget(render_view_host_);

  // TODO(johnnyg): http://crbug.com/23954.  Need a cross-platform solution.
#if defined(OS_WIN)
  RenderWidgetHostViewWin* view_win =
      static_cast<RenderWidgetHostViewWin*>(render_widget_host_view_);

  // Create the HWND.
  HWND hwnd = view_win->Create(parent_native_view_);
  view_win->ShowWindow(SW_SHOW);
  native_host_->Attach(hwnd);
#elif defined(OS_LINUX)
#if defined(TOUCH_UI)
  RenderWidgetHostViewViews* view_views =
      static_cast<RenderWidgetHostViewViews*>(render_widget_host_view_);
  view_views->InitAsChild();
  native_host_->AttachToView(view_views);
#else
  RenderWidgetHostViewGtk* view_gtk =
      static_cast<RenderWidgetHostViewGtk*>(render_widget_host_view_);
  view_gtk->InitAsChild();
  native_host_->Attach(view_gtk->native_view());
#endif
#else
  NOTIMPLEMENTED();
#endif
}

RenderWidgetHostView* BalloonViewHost::render_widget_host_view() const {
  return render_widget_host_view_;
}
