// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_CAPTURE_CONTROLLER_OBSERVER_H_
#define SERVICES_WINDOW_MANAGER_CAPTURE_CONTROLLER_OBSERVER_H_

namespace window_manager {

class CaptureControllerObserver {
 public:
  virtual void OnCaptureChanged(mojo::View* gained_capture) = 0;

 protected:
  virtual ~CaptureControllerObserver() {}
};

}  // namespace window_manager

#endif // SERVICES_WINDOW_MANAGER_CAPTURE_CONTROLLER_OBSERVER_H_
