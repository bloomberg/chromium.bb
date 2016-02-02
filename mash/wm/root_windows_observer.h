// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_ROOT_WINDOW_OBSERVERS_H_
#define MASH_WM_ROOT_WINDOW_OBSERVERS_H_

namespace mash {
namespace wm {

class RootWindowController;

class RootWindowsObserver {
 public:
  // Called once a new display has been created and the root Window obtained.
  virtual void OnRootWindowControllerAdded(
      RootWindowController* controller) = 0;

 protected:
  ~RootWindowsObserver() {}
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_ROOT_WINDOW_OBSERVERS_H_
