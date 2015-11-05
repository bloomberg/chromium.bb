// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/non_client_frame_controller.h"

#include "components/mus/example/wm/non_client_frame_view_impl.h"
#include "components/mus/public/cpp/window.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

namespace {

class WmNativeWidgetMus : public views::NativeWidgetMus {
 public:
  WmNativeWidgetMus(views::internal::NativeWidgetDelegate* delegate,
                    mojo::Shell* shell,
                    mus::Window* window)
      : NativeWidgetMus(delegate,
                        shell,
                        window,
                        mus::mojom::SURFACE_TYPE_UNDERLAY) {}
  ~WmNativeWidgetMus() override {}

  // NativeWidgetMus:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    NonClientFrameViewImpl* frame_view = new NonClientFrameViewImpl(window());
    frame_view->Init(
        static_cast<views::internal::NativeWidgetPrivate*>(this)->GetWidget());
    return frame_view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetMus);
};

}  // namespace

NonClientFrameController::NonClientFrameController(mojo::Shell* shell,
                                                   mus::Window* window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.native_widget = new WmNativeWidgetMus(widget, shell, window);
  widget->Init(params);
  widget->Show();
}

NonClientFrameController::~NonClientFrameController() {}

views::View* NonClientFrameController::GetContentsView() {
  return this;
}
