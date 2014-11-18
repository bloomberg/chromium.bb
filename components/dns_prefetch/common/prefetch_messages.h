// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>
#include <vector>

#include "components/dns_prefetch/common/prefetch_common.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

// Singly-included section for custom IPC traits.
#ifndef COMPONENTS_DNS_PREFETCH_COMMON_PREFETCH_MESSAGES_H_
#define COMPONENTS_DNS_PREFETCH_COMMON_PREFETCH_MESSAGES_H_

namespace IPC {

template <>
struct ParamTraits<dns_prefetch::LookupRequest> {
  typedef dns_prefetch::LookupRequest param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // COMPONENTS_DNS_PREFETCH_COMMON_PREFETCH_MESSAGES_H_

#define IPC_MESSAGE_START DnsPrefetchMsgStart

//-----------------------------------------------------------------------------
// Host messages
// These are messages sent from the renderer process to the browser process.

// Request for a DNS prefetch of the names in the array.
// NameList is typedef'ed std::vector<std::string>
IPC_MESSAGE_CONTROL1(DnsPrefetchMsg_RequestPrefetch,
                     dns_prefetch::LookupRequest)
