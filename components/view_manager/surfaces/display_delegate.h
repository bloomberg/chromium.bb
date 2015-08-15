// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_DELEGATE_H_

namespace surfaces {

class DisplayImpl;

// A DisplayDelegate is an object that tracks the lifetime of a DisplayImpl
// object.
class DisplayDelegate {
 public:
  virtual void OnDisplayCreated(DisplayImpl* display) = 0;
  virtual void OnDisplayConnectionClosed(DisplayImpl* display) = 0;

 protected:
  virtual ~DisplayDelegate() {}
};

}  // namespace surfaces

#endif  // COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_DELEGATE_H_
