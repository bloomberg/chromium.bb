// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_STACKING_CLIENT_H_
#define COMPONENTS_MUS_WS_WINDOW_STACKING_CLIENT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"

namespace mus {
namespace ws {

class ServerWindow;

class WindowStackingClient {
 public:
  // Invoked from the various Window stacking functions. Allows the
  // WindowStackingClient to alter the source, target and/or direction to stack.
  // Returns true if stacking should continue; false if the stacking should not
  // happen.
  virtual bool AdjustStacking(ServerWindow** child,
                              ServerWindow** target,
                              mojom::OrderDirection* direction) = 0;

 protected:
  virtual ~WindowStackingClient() {}
};

// Sets/gets the WindowStackingClient. This does *not* take ownership of
// |client|. It is assumed the caller will invoke SetWindowStackingClient(NULL)
// before deleting |client|.
void SetWindowStackingClient(WindowStackingClient* client);
WindowStackingClient* GetWindowStackingClient();

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_STACKING_CLIENT_H_
