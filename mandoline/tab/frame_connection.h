// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_CONNECTION_H_
#define MANDOLINE_TAB_FRAME_CONNECTION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "mandoline/tab/frame_user_data.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mojo {
class ApplicationConnection;
class ApplicationImpl;
}

namespace mandoline {

// FrameConnection is a FrameUserData that manages the connection to a
// particular frame. It is also responsible for obtaining the necessary
// services from the remote side.
class FrameConnection : public FrameUserData {
 public:
  FrameConnection();
  ~FrameConnection() override;

  void Init(mojo::ApplicationImpl* app,
            mojo::URLRequestPtr request,
            mojo::ViewManagerClientPtr* view_manage_client);

  FrameTreeClient* frame_tree_client() { return frame_tree_client_.get(); }

  mojo::ApplicationConnection* application_connection() {
    return application_connection_.get();
  }

 private:
  FrameTreeClientPtr frame_tree_client_;

  // TODO(sky): needs to be destroyed when connection lost.
  scoped_ptr<mojo::ApplicationConnection> application_connection_;

  DISALLOW_COPY_AND_ASSIGN(FrameConnection);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_CONNECTION_H_
