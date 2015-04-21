// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_LOCATOR_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_LOCATOR_H_

namespace gfx {
class Point;
}

namespace view_manager {

class ServerView;

// Finds the deepest visible view that contains the specified location.
const ServerView* FindDeepestVisibleView(const ServerView* view,
                                         const gfx::Point& location);
ServerView* FindDeepestVisibleView(ServerView* view,
                                   const gfx::Point& location);

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_LOCATOR_H_
