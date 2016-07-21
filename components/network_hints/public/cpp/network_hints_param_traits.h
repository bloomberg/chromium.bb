// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_HINTS_COMMON_NETWORK_HINTS_PARAM_TRAITS_H_
#define COMPONENTS_NETWORK_HINTS_COMMON_NETWORK_HINTS_PARAM_TRAITS_H_

#include <string>
#include <vector>

#include "components/network_hints/common/network_hints_common.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "url/ipc/url_param_traits.h"

namespace IPC {

template <>
struct ParamTraits<network_hints::LookupRequest> {
  typedef network_hints::LookupRequest param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // COMPONENTS_NETWORK_HINTS_COMMON_NETWORK_HINTS_PARAM_TRAITS_H_
