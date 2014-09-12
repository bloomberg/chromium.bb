// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_SERVER_VIEW_H_
#define MOJO_SERVICES_VIEW_MANAGER_SERVER_VIEW_H_

#include <vector>

#include "base/logging.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace service {

class ServerViewDelegate;

// Server side representation of a view. Delegate is informed of interesting
// events.
//
// It is assumed that all functions that mutate the tree have validated the
// mutation is possible before hand. For example, Reorder() assumes the supplied
// view is a child and not already in position.
class MOJO_VIEW_MANAGER_EXPORT ServerView {
 public:
  ServerView(ServerViewDelegate* delegate, const ViewId& id);
  virtual ~ServerView();

  const ViewId& id() const { return id_; }

  void Add(ServerView* child);
  void Remove(ServerView* child);
  void Reorder(ServerView* child,
               ServerView* relative,
               OrderDirection direction);

  const gfx::Rect& bounds() const { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  const ServerView* parent() const { return parent_; }
  ServerView* parent() { return parent_; }

  const ServerView* GetRoot() const;
  ServerView* GetRoot() {
    return const_cast<ServerView*>(
        const_cast<const ServerView*>(this)->GetRoot());
  }

  std::vector<const ServerView*> GetChildren() const;
  std::vector<ServerView*> GetChildren();

  // Returns true if this contains |view| or is |view|.
  bool Contains(const ServerView* view) const;

  // Returns true if the window is visible. This does not consider visibility
  // of any ancestors.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  // Returns true if this view is attached to |root| and all ancestors are
  // visible.
  bool IsDrawn(const ServerView* root) const;

  void SetSurfaceId(cc::SurfaceId surface_id);
  const cc::SurfaceId surface_id() const { return surface_id_; }

 private:
  typedef std::vector<ServerView*> Views;

  // Implementation of removing a view. Doesn't send any notification.
  void RemoveImpl(ServerView* view);

  ServerViewDelegate* delegate_;
  const ViewId id_;
  ServerView* parent_;
  Views children_;
  bool visible_;
  gfx::Rect bounds_;
  cc::SurfaceId surface_id_;

  DISALLOW_COPY_AND_ASSIGN(ServerView);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_SERVER_VIEW_H_
