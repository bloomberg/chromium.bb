// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_H_

#include <vector>

#include "base/logging.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

class Node;

// Represents a view. A view may be associated with a single Node.
class MOJO_VIEW_MANAGER_EXPORT View {
 public:
  explicit View(const ViewId& id);
  ~View();

  const ViewId& id() const { return id_; }

  Node* node() { return node_; }

 private:
  // Node is responsible for maintaining |node_|.
  friend class Node;

  void set_node(Node* node) { node_ = node; }

  const ViewId id_;
  Node* node_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
