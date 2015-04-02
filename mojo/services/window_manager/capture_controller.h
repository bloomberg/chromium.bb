// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_CAPTURE_CONTROLLER_H_
#define SERVICES_WINDOW_MANAGER_CAPTURE_CONTROLLER_H_

#include "base/observer_list.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_observer.h"

namespace window_manager {

class CaptureControllerObserver;

// Manages input capture. A view which has capture will take all input events.
class CaptureController : public mojo::ViewObserver {
 public:
  CaptureController();
  ~CaptureController() override;

  void AddObserver(CaptureControllerObserver* observer);
  void RemoveObserver(CaptureControllerObserver* observer);

  void SetCapture(mojo::View* view);
  void ReleaseCapture(mojo::View* view);
  mojo::View* GetCapture();

 private:
  void NotifyCaptureChange(mojo::View* new_capture, mojo::View* old_capture);

  // Overridden from ViewObserver:
  void OnViewDestroying(mojo::View* view) override;

  // The current capture view. Null if there is no capture view.
  mojo::View* capture_view_;

  ObserverList<CaptureControllerObserver> capture_controller_observers_;

  DISALLOW_COPY_AND_ASSIGN(CaptureController);
};

void SetCaptureController(mojo::View* view,
                          CaptureController* capture_controller);
CaptureController* GetCaptureController(mojo::View* view);
mojo::View* GetCaptureView(mojo::View* view);

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_CAPTURE_CONTROLLER_H_
