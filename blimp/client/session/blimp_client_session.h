// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_
#define BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "blimp/client/blimp_client_export.h"
#include "blimp/client/session/assignment_source.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"

namespace net {
class IPEndPoint;
}

namespace blimp {

class BlimpMessageProcessor;
class BlimpMessageThreadPipe;
class BrowserConnectionHandler;
class ClientConnectionManager;
class ThreadPipeManager;

namespace client {

class ClientNetworkComponents;
class NavigationFeature;
class RenderWidgetFeature;
class TabControlFeature;

// BlimpClientSession represents a single active session of Blimp on the client
// regardless of whether or not the client application is in the background or
// foreground.  The only time this session is invalid is during initialization
// and shutdown of this particular client process (or Activity on Android).
//
// This session glues together the feature proxy components and the network
// layer.  The network components must be interacted with on the IO thread.  The
// feature proxies must be interacted with on the UI thread.
class BLIMP_CLIENT_EXPORT BlimpClientSession {
 public:
  BlimpClientSession();

  // Uses the AssignmentSource to get an Assignment and then uses the assignment
  // configuration to connect to the Blimplet.
  // |client_auth_token| is the OAuth2 access token to use when querying
  // for an assignment.  This token needs the OAuth2 scope of userinfo.email and
  // only needs to be an access token, not a refresh token.
  void Connect(const std::string& client_auth_token);

  TabControlFeature* GetTabControlFeature() const;
  NavigationFeature* GetNavigationFeature() const;
  RenderWidgetFeature* GetRenderWidgetFeature() const;

  // The AssignmentCallback for when an assignment is ready. This will trigger
  // a connection to the engine.
  virtual void ConnectWithAssignment(AssignmentSource::Result result,
                                     const Assignment& assignment);

 protected:
  virtual ~BlimpClientSession();

  // Notified every time the AssignmentSource returns the result of an attempted
  // assignment request.
  virtual void OnAssignmentConnectionAttempted(AssignmentSource::Result result);

 private:
  void RegisterFeatures();

  base::Thread io_thread_;
  scoped_ptr<TabControlFeature> tab_control_feature_;
  scoped_ptr<NavigationFeature> navigation_feature_;
  scoped_ptr<RenderWidgetFeature> render_widget_feature_;

  // The AssignmentSource is used when the user of BlimpClientSession calls
  // Connect() to get a valid assignment and later connect to the engine.
  scoped_ptr<AssignmentSource> assignment_source_;

  // Container struct for network components.
  // Must be deleted on the IO thread.
  scoped_ptr<ClientNetworkComponents> net_components_;

  scoped_ptr<ThreadPipeManager> thread_pipe_manager_;

  base::WeakPtrFactory<BlimpClientSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSession);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_
