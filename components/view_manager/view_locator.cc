// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_locator.h"

#include "components/view_manager/server_view.h"

namespace view_manager {

const ServerView* FindDeepestVisibleView(const ServerView* view,
                                         const gfx::Point& location) {
  for (const ServerView* child : view->GetChildren()) {
    if (!child->visible())
      continue;

    // TODO(sky): support transform.
    const gfx::Point child_location(location.x() - child->bounds().x(),
                                    location.y() - child->bounds().y());
    if (child_location.x() >= 0 && child_location.y() >= 0 &&
        child_location.x() < child->bounds().width() &&
        child_location.y() < child->bounds().height()) {
      return FindDeepestVisibleView(child, child_location);
    }
  }
  return view;
}

ServerView* FindDeepestVisibleView(ServerView* view,
                                   const gfx::Point& location) {
  return const_cast<ServerView*>(
      FindDeepestVisibleView(const_cast<const ServerView*>(view), location));
}

}  // namespace view_manager
