// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_DEADLINE_OBSERVER_H_
#define CC_SURFACES_SURFACE_DEADLINE_OBSERVER_H_

namespace cc {

class SurfaceDeadlineObserver {
 public:
  // Called when a deadline passes for a set of dependencies.
  virtual void OnDeadline() = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_DEADLINE_OBSERVER_H_
