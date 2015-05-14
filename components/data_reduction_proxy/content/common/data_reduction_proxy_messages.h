// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/host_port_pair.h"

#define IPC_MESSAGE_START DataReductionProxyStart

IPC_ENUM_TRAITS_MAX_VALUE(data_reduction_proxy::AutoLoFiStatus,
                          data_reduction_proxy::AUTO_LOFI_STATUS_LAST);

IPC_SYNC_MESSAGE_CONTROL1_2(
    DataReductionProxyViewHostMsg_DataReductionProxyStatus,
    net::HostPortPair /* proxy server */,
    bool /* true iff the proxy server is a Data Reduction Proxy */,
    data_reduction_proxy::AutoLoFiStatus
    /* set to the current Auto LoFi status */)
