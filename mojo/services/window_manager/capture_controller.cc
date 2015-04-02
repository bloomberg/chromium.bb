// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/capture_controller.h"

#include "mojo/services/window_manager/capture_controller_observer.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_property.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_tracker.h"

DECLARE_VIEW_PROPERTY_TYPE(window_manager::CaptureController*);

namespace window_manager {

namespace {
DEFINE_VIEW_PROPERTY_KEY(CaptureController*,
                         kRootViewCaptureController,
                         nullptr);
}  // namespace

CaptureController::CaptureController()
    : capture_view_(nullptr) {}

CaptureController::~CaptureController() {}

void CaptureController::AddObserver(CaptureControllerObserver* observer) {
  capture_controller_observers_.AddObserver(observer);
}

void CaptureController::RemoveObserver(CaptureControllerObserver* observer) {
  capture_controller_observers_.RemoveObserver(observer);
}

void CaptureController::SetCapture(mojo::View* view) {
  if (capture_view_ == view)
    return;

  if (capture_view_)
    capture_view_->RemoveObserver(this);

  mojo::View* old_capture_view = capture_view_;
  capture_view_ = view;

  if (capture_view_)
    capture_view_->AddObserver(this);

  NotifyCaptureChange(capture_view_, old_capture_view);
}

void CaptureController::ReleaseCapture(mojo::View* view) {
  if (capture_view_ != view)
    return;
  SetCapture(nullptr);
}

mojo::View* CaptureController::GetCapture() {
  return capture_view_;
}

void CaptureController::NotifyCaptureChange(mojo::View* new_capture,
                                            mojo::View* old_capture) {
  mojo::ViewTracker view_tracker;
  if (new_capture)
    view_tracker.Add(new_capture);
  if (old_capture)
    view_tracker.Add(old_capture);

  FOR_EACH_OBSERVER(
      CaptureControllerObserver, capture_controller_observers_,
      OnCaptureChanged(view_tracker.Contains(new_capture) ? new_capture
                                                          : nullptr));
}

void CaptureController::OnViewDestroying(mojo::View* view) {
  if (view == capture_view_) {
    view->RemoveObserver(this);
    NotifyCaptureChange(nullptr, view);
    capture_view_ = nullptr;
  }
}

void SetCaptureController(mojo::View* root_view,
                          CaptureController* capture_controller) {
  DCHECK_EQ(root_view->GetRoot(), root_view);
  root_view->SetLocalProperty(kRootViewCaptureController, capture_controller);
}

CaptureController* GetCaptureController(mojo::View* root_view) {
  if (root_view)
    DCHECK_EQ(root_view->GetRoot(), root_view);
  return root_view ?
      root_view->GetLocalProperty(kRootViewCaptureController) : nullptr;
}

mojo::View* GetCaptureView(mojo::View* view) {
  mojo::View* root = view->GetRoot();
  if (!root)
    return nullptr;
  CaptureController* controller = GetCaptureController(root);
  return controller ? controller->GetCapture() : nullptr;
}

}  // namespace window_manager
