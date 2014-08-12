// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"

namespace gfx {
class Rect;
}

namespace mojo {

class View;

// A note on -ing and -ed suffixes:
//
// -ing methods are called before changes are applied to the local view model.
// -ed methods are called after changes are applied to the local view model.
//
// If the change originated from another connection to the view manager, it's
// possible that the change has already been applied to the service-side model
// prior to being called, so for example in the case of OnViewDestroying(), it's
// possible the view has already been destroyed on the service side.

class ViewObserver {
 public:
  struct TreeChangeParams {
    TreeChangeParams();
    View* target;
    View* old_parent;
    View* new_parent;
    View* receiver;
  };

  virtual void OnTreeChanging(const TreeChangeParams& params) {}
  virtual void OnTreeChanged(const TreeChangeParams& params) {}

  virtual void OnViewReordering(View* view,
                                View* relative_view,
                                OrderDirection direction) {}
  virtual void OnViewReordered(View* view,
                               View* relative_view,
                               OrderDirection direction) {}

  virtual void OnViewDestroying(View* view) {}
  virtual void OnViewDestroyed(View* view) {}

  virtual void OnViewBoundsChanging(View* view,
                                    const gfx::Rect& old_bounds,
                                    const gfx::Rect& new_bounds) {}
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) {}

  virtual void OnViewFocusChanged(View* gained_focus, View* lost_focus) {}

  virtual void OnViewInputEvent(View* view, const EventPtr& event) {}

 protected:
  virtual ~ViewObserver() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_
