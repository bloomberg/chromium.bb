// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_JAVA_BRIDGE_CHANNEL_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_JAVA_BRIDGE_CHANNEL_HOST_H_
#pragma once

#include "content/common/np_channel_base.h"

class JavaBridgeChannelHost : public NPChannelBase {
 public:
  static JavaBridgeChannelHost* GetJavaBridgeChannelHost(
      int renderer_id, base::MessageLoopProxy*);

  // NPChannelBase implementation:
  virtual int GenerateRouteID() OVERRIDE;

  // NPChannelBase override:
  virtual bool Init(base::MessageLoopProxy* ipc_message_loop,
                    bool create_pipe_now,
                    base::WaitableEvent* shutdown_event) OVERRIDE;
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  JavaBridgeChannelHost() {}
  friend class base::RefCountedThreadSafe<JavaBridgeChannelHost>;
  virtual ~JavaBridgeChannelHost() {}

  static NPChannelBase* ClassFactory() {
    return new JavaBridgeChannelHost();
  }

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeChannelHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_JAVA_BRIDGE_CHANNEL_HOST_H_
