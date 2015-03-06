// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTERNAL_IPC_DUMPER_H_
#define CHROME_COMMON_EXTERNAL_IPC_DUMPER_H_

#include "ipc/ipc_channel_proxy.h"

IPC::ChannelProxy::OutgoingMessageFilter* LoadExternalIPCDumper(
    const base::FilePath& dump_directory);

#endif  // CHROME_COMMON_EXTERNAL_IPC_DUMPER_H_
