// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_CONNECTION_H_
#define COMPONENTS_WEB_VIEW_FRAME_CONNECTION_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mojo {
class ApplicationConnection;
class Shell;
}

namespace web_view {

class Frame;

// FrameConnection is a FrameUserData that manages the connection to a
// particular frame. It is also responsible for obtaining the necessary
// services from the remote side.
class FrameConnection : public FrameUserData {
 public:
  FrameConnection();
  ~FrameConnection() override;

  // Creates a FrameConnection to service a call from
  // FrameTreeDelegate::CanNavigateFrame(). |callback| is run once the
  // content handler id for the app is determined.
  static void CreateConnectionForCanNavigateFrame(
      mojo::Shell* shell,
      Frame* frame,
      mojo::URLRequestPtr request,
      const FrameTreeDelegate::CanNavigateFrameCallback& callback);

  // Initializes the FrameConnection with the specified parameters.
  // |on_got_id_callback| is run once the content handler is obtained from
  // the connection.
  void Init(mojo::Shell* shell,
            mojo::URLRequestPtr request,
            const base::Closure& on_got_id_callback);

  mojom::FrameClient* frame_client() { return frame_client_.get(); }

  mojo::ApplicationConnection* application_connection() {
    return application_connection_.get();
  }

  // Asks the remote application for a WindowTreeClient.
  mus::mojom::WindowTreeClientPtr GetWindowTreeClient();

  // Returns the id of the content handler app. Only available once the callback
  // has run.
  uint32_t GetContentHandlerID() const;

 private:
  mojom::FrameClientPtr frame_client_;

  scoped_ptr<mojo::ApplicationConnection> application_connection_;

  DISALLOW_COPY_AND_ASSIGN(FrameConnection);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_CONNECTION_H_
