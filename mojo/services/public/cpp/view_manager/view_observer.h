// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"

namespace mojo {
namespace view_manager {

class View;

class ViewObserver {
 public:
  enum DispositionChangePhase {
    DISPOSITION_CHANGING,
    DISPOSITION_CHANGED
  };

  virtual void OnViewDestroy(View* view, DispositionChangePhase phase) {}

 protected:
  virtual ~ViewObserver() {}
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_
