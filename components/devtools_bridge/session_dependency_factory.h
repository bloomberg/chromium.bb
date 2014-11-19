// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_SESSION_DEPENDENCY_FACTORY_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_SESSION_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "components/devtools_bridge/abstract_peer_connection.h"
#include "components/devtools_bridge/rtc_configuration.h"

namespace base {
class TaskRunner;
}

namespace devtools_bridge {

/**
 * Abstraction layer over WebRTC API used in DevTools Bridge.
 * It helps to isolate ref counting and threading logic needed.
 */
class SessionDependencyFactory {
 public:
  SessionDependencyFactory() {}
  virtual ~SessionDependencyFactory() {}

  static bool InitializeSSL();
  static bool CleanupSSL();

  static scoped_ptr<SessionDependencyFactory> CreateInstance(
      const base::Closure& cleanup_on_signaling_thread);

  virtual scoped_ptr<AbstractPeerConnection> CreatePeerConnection(
      scoped_ptr<RTCConfiguration> config,
      scoped_ptr<AbstractPeerConnection::Delegate> delegate) = 0;

  virtual scoped_refptr<base::TaskRunner> signaling_thread_task_runner() = 0;
  virtual scoped_refptr<base::TaskRunner> io_thread_task_runner() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionDependencyFactory);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_SESSION_DEPENDENCY_FACTORY_H_
