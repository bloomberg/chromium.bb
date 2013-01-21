// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_METRO_VIEWER_PROCESS_HOST_H_
#define ASH_TEST_TEST_METRO_VIEWER_PROCESS_HOST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"

class AcceleratedSurface;

namespace ash {
namespace test {

class TestMetroViewerProcessHost : public IPC::Listener,
                                   public IPC::Sender,
                                   public base::NonThreadSafe {
 public:
  explicit TestMetroViewerProcessHost(const std::string& ipc_channel_name);
  virtual ~TestMetroViewerProcessHost();

  // Launches the Chrome viewer process and blocks until that viewer process
  // connects or until a timeout is reached. Returns true if the viewer process
  // connects before the timeout is reached.
  // TODO(robertshield): This creates a run-time dependency on chrome.exe as the
  // viewer process and, indirectly, setup.exe as the only thing that can
  // correctly register a program as the default browser on metro. Investigate
  // extracting the registration code and the metro init code and building them
  // into a standalone viewer process.
  bool LaunchImmersiveChromeAndWaitForConnection();

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  bool closed_unexpectedly() { return closed_unexpectedly_; }

 private:
  void OnSetTargetSurface(gfx::NativeViewId target_surface);

  void NotifyChannelConnected();

  // Inner message filter used to handle connection event on the IPC channel
  // proxy's background thread. This prevents consumers of
  // TestMetroViewerProcessHost from having to pump messages on their own
  // message loop.
  class InternalMessageFilter : public IPC::ChannelProxy::MessageFilter {
   public:
    InternalMessageFilter(TestMetroViewerProcessHost* owner);

    // IPC::ChannelProxy::MessageFilter implementation.
    virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

   private:
    TestMetroViewerProcessHost* owner_;
    DISALLOW_COPY_AND_ASSIGN(InternalMessageFilter);
  };

  // Members related to the IPC channel. Note that the order is important
  // here as ipc_thread_ should be destroyed after channel_.
  base::Thread ipc_thread_;
  scoped_ptr<IPC::ChannelProxy> channel_;
  base::WaitableEvent channel_connected_event_;

  scoped_ptr<AcceleratedSurface> backing_surface;

  bool closed_unexpectedly_;

  DISALLOW_COPY_AND_ASSIGN(TestMetroViewerProcessHost);
};


}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_METRO_VIEWER_PROCESS_HOST_H_
