// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_SCOPED_VIEW_PTR_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_SCOPED_VIEW_PTR_H_

#include "components/view_manager/public/cpp/view_observer.h"

namespace mojo {

// Wraps a View, taking overship of the View. Also deals with View being
// destroyed while ScopedViewPtr still exists.
class ScopedViewPtr : public ViewObserver {
 public:
  explicit ScopedViewPtr(View* view);
  ~ScopedViewPtr() override;

  // Destroys |view|. If |view| is the root of the ViewManager than the
  // ViewManager is destroyed (which in turn destroys |view|).
  static void DeleteViewOrViewManager(View* view);

  View* view() { return view_; }
  const View* view() const { return view_; }

 private:
  void DetachFromView();

  void OnViewDestroying(View* view) override;

  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ScopedViewPtr);
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_SCOPED_VIEW_PTR_H_
