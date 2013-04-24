// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRO_VIEWER_METRO_VIEWER_PROCESS_HOST_WIN_H_
#define CHROME_BROWSER_METRO_VIEWER_METRO_VIEWER_PROCESS_HOST_WIN_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"

namespace IPC {
class ChannelProxy;
}

class MetroViewerProcessHost : public IPC::Listener,
                               public IPC::Sender,
                               public base::NonThreadSafe {
 public:
  explicit MetroViewerProcessHost(const std::string& ipc_channel_name);
  virtual ~MetroViewerProcessHost();

 private:
  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  void OnSetTargetSurface(gfx::NativeViewId target_surface);

  scoped_ptr<IPC::ChannelProxy> channel_;

  DISALLOW_COPY_AND_ASSIGN(MetroViewerProcessHost);
};

#endif  // CHROME_BROWSER_METRO_VIEWER_METRO_VIEWER_PROCESS_HOST_WIN_H_
