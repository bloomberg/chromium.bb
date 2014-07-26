// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ACCESS_POLICY_DELEGATE_H_
#define MOJO_SERVICES_VIEW_MANAGER_ACCESS_POLICY_DELEGATE_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "mojo/services/view_manager/ids.h"

namespace mojo {
namespace service {

class Node;

// Delegate used by the AccessPolicy implementations to get state.
class AccessPolicyDelegate {
 public:
  // Returns the ids of the roots nodes for this connection. That is, this is
  // the set of nodes the connection was embedded at.
  virtual const base::hash_set<Id>& GetRootsForAccessPolicy() const = 0;

  // Returns true if |node| has been exposed to the client.
  virtual bool IsNodeKnownForAccessPolicy(const Node* node) const = 0;

  // Returns true if Embed(node) has been invoked on |node|.
  virtual bool IsNodeRootOfAnotherConnectionForAccessPolicy(
      const Node* node) const = 0;

 protected:
  virtual ~AccessPolicyDelegate() {}
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ACCESS_POLICY_DELEGATE_H_
