// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_NODE_H_
#define MOJO_SERVICES_VIEW_MANAGER_NODE_H_

#include <vector>

#include "base/logging.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace service {

class NodeDelegate;

// Represents a node in the graph. Delegate is informed of interesting events.
//
// It is assumed that all functions that mutate the node tree have validated the
// value. For example, Reorder() assumes the supplied node is a child and not
// already in position.
class MOJO_VIEW_MANAGER_EXPORT Node {
 public:
  Node(NodeDelegate* delegate, const NodeId& id);
  virtual ~Node();

  const NodeId& id() const { return id_; }

  void Add(Node* child);
  void Remove(Node* child);
  void Reorder(Node* child, Node* relative, OrderDirection direction);

  const gfx::Rect& bounds() const { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  const Node* parent() const { return parent_; }
  Node* parent() { return parent_; }

  const Node* GetRoot() const;
  Node* GetRoot() {
    return const_cast<Node*>(const_cast<const Node*>(this)->GetRoot());
  }

  std::vector<const Node*> GetChildren() const;
  std::vector<Node*> GetChildren();

  bool Contains(const Node* node) const;

  // Returns true if the window is visible. This does not consider visibility
  // of any ancestors.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  void SetBitmap(const SkBitmap& contents);
  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  typedef std::vector<Node*> Nodes;

  // Implementation of removing a node. Doesn't send any notification.
  void RemoveImpl(Node* node);

  NodeDelegate* delegate_;
  const NodeId id_;
  Node* parent_;
  Nodes children_;
  bool visible_;
  gfx::Rect bounds_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_NODE_H_
