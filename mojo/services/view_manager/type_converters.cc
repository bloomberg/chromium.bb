// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/type_converters.h"

#include "mojo/public/cpp/bindings/buffer.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/view.h"

using mojo::view_manager::INode;
using mojo::view_manager::service::Node;
using mojo::view_manager::service::NodeId;
using mojo::view_manager::service::ViewId;

namespace mojo {

// static
INode TypeConverter<INode, const Node*>::ConvertFrom(const Node* node,
                                                     Buffer* buf) {
  DCHECK(node);

  INode::Builder builder(buf);
  const Node* parent = node->GetParent();
  builder.set_parent_id(NodeIdToTransportId(parent ? parent->id() : NodeId()));
  builder.set_node_id(NodeIdToTransportId(node->id()));
  builder.set_view_id(ViewIdToTransportId(
                          node->view() ? node->view()->id() : ViewId()));
  return builder.Finish();
}

}  // namespace mojo
