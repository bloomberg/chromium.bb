// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_OBSERVER_H_
#define ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_OBSERVER_H_

#include "athena/athena_export.h"

namespace aura {
class Window;
}

namespace athena {

// An observer to window list changes like e.g. stacking order has changed.
class ATHENA_EXPORT WindowListProviderObserver {
 public:
  virtual ~WindowListProviderObserver() {}

  // The Window stacking has changed.
  virtual void OnWindowStackingChanged() = 0;

  // The specified Window has been removed from the list. At the time of
  // removal, |removed_window| was at the |index| position in the window list.
  virtual void OnWindowRemoved(aura::Window* removed_window,
                               int index) = 0;
};

}  // namespace athena

#endif  // ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_OBSERVER_H_
