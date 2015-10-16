// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_ACCESS_POLICY_DELEGATE_H_
#define COMPONENTS_MUS_WS_ACCESS_POLICY_DELEGATE_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "components/mus/ws/ids.h"

namespace mus {

class ServerView;

// Delegate used by the AccessPolicy implementations to get state.
class AccessPolicyDelegate {
 public:
  // Returns true if |id| is the root of the connection.
  virtual bool IsRootForAccessPolicy(const ViewId& id) const = 0;

  // Returns true if |view| has been exposed to the client.
  virtual bool IsViewKnownForAccessPolicy(const ServerView* view) const = 0;

  // Returns true if Embed(view) has been invoked on |view|.
  virtual bool IsViewRootOfAnotherConnectionForAccessPolicy(
      const ServerView* view) const = 0;

  // Returns true if SetEmbedRoot() was invoked and |view| is a descendant of
  // the root of the connection.
  virtual bool IsDescendantOfEmbedRoot(const ServerView* view) = 0;

 protected:
  virtual ~AccessPolicyDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_ACCESS_POLICY_DELEGATE_H_
