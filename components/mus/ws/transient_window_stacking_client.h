// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TRANSIENT_WINDOW_STACKING_CLIENT_H_
#define COMPONENTS_MUS_WS_TRANSIENT_WINDOW_STACKING_CLIENT_H_

#include "components/mus/ws/window_stacking_client.h"

#include "base/macros.h"

namespace mus {
namespace ws {

class TransientWindowManager;

class TransientWindowStackingClient : public WindowStackingClient {
 public:
  TransientWindowStackingClient();
  ~TransientWindowStackingClient() override;

  // WindowStackingClient:
  bool AdjustStacking(ServerWindow** child,
                      ServerWindow** target,
                      mojom::OrderDirection* direction) override;

 private:
  // Purely for DCHECKs.
  friend class TransientWindowManager;

  static TransientWindowStackingClient* instance_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowStackingClient);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TRANSIENT_WINDOW_STACKING_CLIENT_H_
