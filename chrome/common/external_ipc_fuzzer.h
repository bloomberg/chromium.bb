// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTERNAL_IPC_FUZZER_H_
#define CHROME_COMMON_EXTERNAL_IPC_FUZZER_H_

#include "ipc/ipc_channel_proxy.h"

IPC::ChannelProxy::OutgoingMessageFilter* LoadExternalIPCFuzzer();

#endif  // CHROME_COMMON_EXTERNAL_IPC_FUZZER_H_

