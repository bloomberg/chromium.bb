// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_VIEW_OBSERVER_H_
#define COMPONENTS_MUS_WS_SERVER_VIEW_OBSERVER_H_

#include "components/mus/public/interfaces/mus_constants.mojom.h"

namespace gfx {
class Rect;
}

namespace mus {
class ViewportMetrics;
}

namespace ui {
struct TextInputState;
}

namespace mus {

class ServerView;

// TODO(sky): rename to OnDid and OnWill everywhere.
class ServerViewObserver {
 public:
  // Invoked when a view is about to be destroyed; before any of the children
  // have been removed and before the view has been removed from its parent.
  virtual void OnWillDestroyView(ServerView* view) {}

  // Invoked at the end of the View's destructor (after it has been removed from
  // the hierarchy).
  virtual void OnViewDestroyed(ServerView* view) {}

  virtual void OnWillChangeViewHierarchy(ServerView* view,
                                         ServerView* new_parent,
                                         ServerView* old_parent) {}

  virtual void OnViewHierarchyChanged(ServerView* view,
                                      ServerView* new_parent,
                                      ServerView* old_parent) {}

  virtual void OnViewBoundsChanged(ServerView* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) {}

  virtual void OnWindowClientAreaChanged(ServerView* window,
                                         const gfx::Rect& old_client_area,
                                         const gfx::Rect& new_client_area) {}

  virtual void OnViewReordered(ServerView* view,
                               ServerView* relative,
                               mojom::OrderDirection direction) {}

  virtual void OnWillChangeViewVisibility(ServerView* view) {}
  virtual void OnViewVisibilityChanged(ServerView* view) {}

  virtual void OnViewTextInputStateChanged(ServerView* view,
                                           const ui::TextInputState& state) {}

  virtual void OnViewSharedPropertyChanged(
      ServerView* view,
      const std::string& name,
      const std::vector<uint8_t>* new_data) {}

 protected:
  virtual ~ServerViewObserver() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_VIEW_OBSERVER_H_
