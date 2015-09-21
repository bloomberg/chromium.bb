// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_CLIENT_INITIATED_FRAME_CONNECTION_H_
#define COMPONENTS_WEB_VIEW_CLIENT_INITIATED_FRAME_CONNECTION_H_

#include "base/basictypes.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/public/interfaces/frame.mojom.h"

namespace web_view {

// FrameUserData for use when a Frame is created by way of OnCreatedFrame(). In
// this case the FrameClient is supplied from Frame that called
// OnCreatedFrame().
class ClientInitiatedFrameConnection : public FrameUserData {
 public:
  explicit ClientInitiatedFrameConnection(mojom::FrameClientPtr frame_client);
  ~ClientInitiatedFrameConnection() override;

 private:
  mojom::FrameClientPtr frame_client_;

  DISALLOW_COPY_AND_ASSIGN(ClientInitiatedFrameConnection);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_CLIENT_INITIATED_FRAME_CONNECTION_H_
