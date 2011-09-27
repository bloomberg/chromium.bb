// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_JAVA_BRIDGE_CHANNEL_H_
#define CHROME_RENDERER_JAVA_BRIDGE_CHANNEL_H_
#pragma once

#include "content/common/np_channel_base.h"
#include "ipc/ipc_channel_handle.h"

class JavaBridgeChannel : public NPChannelBase {
 public:
  static JavaBridgeChannel* GetJavaBridgeChannel(
      const IPC::ChannelHandle& channel_handle,
      base::MessageLoopProxy* ipc_message_loop);
  virtual ~JavaBridgeChannel() {}

  // NPChannelBase implementation:
  virtual int GenerateRouteID() OVERRIDE;

  // NPChannelBase override:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  JavaBridgeChannel() {}

  static NPChannelBase* ClassFactory() { return new JavaBridgeChannel(); }

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeChannel);
};

#endif  // CHROME_RENDERER_JAVA_BRIDGE_CHANNEL_H_
