// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_ACCESS_POLICY_H_
#define COMPONENTS_MUS_WS_ACCESS_POLICY_H_

#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/ws/ids.h"

namespace mus {

class ServerView;

// AccessPolicy is used by ViewTreeImpl to determine what a connection is
// allowed to do.
class AccessPolicy {
 public:
  virtual ~AccessPolicy() {}

  // Unless otherwise mentioned all arguments have been validated. That is the
  // |view| arguments are non-null unless otherwise stated (eg CanSetWindow() is
  // allowed to take a NULL view).
  virtual bool CanRemoveWindowFromParent(const ServerView* view) const = 0;
  virtual bool CanAddWindow(const ServerView* parent,
                            const ServerView* child) const = 0;
  virtual bool CanReorderWindow(const ServerView* view,
                                const ServerView* relative_view,
                                mojom::OrderDirection direction) const = 0;
  virtual bool CanDeleteWindow(const ServerView* view) const = 0;
  virtual bool CanGetWindowTree(const ServerView* view) const = 0;
  // Used when building a view tree (GetViewTree()) to decide if we should
  // descend into |view|.
  virtual bool CanDescendIntoViewForViewTree(const ServerView* view) const = 0;
  virtual bool CanEmbed(const ServerView* view,
                        uint32_t policy_bitmask) const = 0;
  virtual bool CanChangeViewVisibility(const ServerView* view) const = 0;
  virtual bool CanSetWindowSurfaceId(const ServerView* view) const = 0;
  virtual bool CanSetWindowBounds(const ServerView* view) const = 0;
  virtual bool CanSetWindowProperties(const ServerView* view) const = 0;
  virtual bool CanSetWindowTextInputState(const ServerView* view) const = 0;
  virtual bool CanSetFocus(const ServerView* view) const = 0;
  virtual bool CanSetClientArea(const ServerView* window) const = 0;

  // Returns whether the connection should notify on a hierarchy change.
  // |new_parent| and |old_parent| are initially set to the new and old parents
  // but may be altered so that the client only sees a certain set of views.
  virtual bool ShouldNotifyOnHierarchyChange(
      const ServerView* view,
      const ServerView** new_parent,
      const ServerView** old_parent) const = 0;

  // Returns the view to supply to the client when focus changes to |focused|.
  virtual const ServerView* GetViewForFocusChange(
      const ServerView* focused) = 0;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_ACCESS_POLICY_H_
