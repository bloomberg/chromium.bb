// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/time/time.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START StartupMetricMsgStart

IPC_MESSAGE_CONTROL1(StartupMetricHostMsg_RecordRendererMainEntryTime,
                     base::TimeTicks /* renderer_main_entry_time */)
