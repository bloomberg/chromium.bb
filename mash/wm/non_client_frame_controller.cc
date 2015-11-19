// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/non_client_frame_controller.h"

#include "components/mus/public/cpp/window.h"
#include "mash/wm/frame/non_client_frame_view_mash.h"
#include "mash/wm/property_util.h"
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
    views::Widget* widget =
        static_cast<views::internal::NativeWidgetPrivate*>(this)->GetWidget();
    mash::wm::NonClientFrameViewMash* frame_view =
        new mash::wm::NonClientFrameViewMash(widget, window());
    return frame_view;
  }
  void CenterWindow(const gfx::Size& size) override {
    // Do nothing. The client controls the size, not us.
  }
  void UpdateClientArea() override {
    // This pushes the client area to the WS. We don't want to do that as
    // the client area should come from the client, not us.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetMus);
};

}  // namespace

NonClientFrameController::NonClientFrameController(mojo::Shell* shell,
                                                   mus::Window* window)
    : widget_(new views::Widget), window_(window) {
  window_->AddObserver(this);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.native_widget = new WmNativeWidgetMus(widget_, shell, window);
  widget_->Init(params);
  widget_->Show();
}

// static
gfx::Insets NonClientFrameController::GetPreferredClientAreaInsets() {
  return mash::wm::NonClientFrameViewMash::GetPreferredClientAreaInsets();
}

NonClientFrameController::~NonClientFrameController() {
  if (window_)
    window_->RemoveObserver(this);
}

views::View* NonClientFrameController::GetContentsView() {
  return this;
}

bool NonClientFrameController::CanResize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::RESIZE_BEHAVIOR_CAN_RESIZE) != 0;
}

bool NonClientFrameController::CanMaximize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::RESIZE_BEHAVIOR_CAN_MAXIMIZE) != 0;
}

bool NonClientFrameController::CanMinimize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::RESIZE_BEHAVIOR_CAN_MINIMIZE) != 0;
}

void NonClientFrameController::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == mus::mojom::WindowManager::kResizeBehavior_Property)
    widget_->OnSizeConstraintsChanged();
}

void NonClientFrameController::OnWindowDestroyed(mus::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}
