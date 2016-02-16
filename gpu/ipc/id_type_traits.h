// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_ID_TYPE_TRAITS_H_
#define GPU_IPC_ID_TYPE_TRAITS_H_

#include <string>

#include "base/pickle.h"
#include "gpu/command_buffer/common/id_type.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"

namespace IPC {

template <typename TypeMarker, typename WrappedType, WrappedType kInvalidValue>
struct ParamTraits<gpu::IdType<TypeMarker, WrappedType, kInvalidValue>> {
  using param_type = gpu::IdType<TypeMarker, WrappedType, kInvalidValue>;
  static void GetSize(base::PickleSizer* sizer, const param_type& p) {
    GetParamSize(sizer, p.GetUnsafeValue());
  }
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.GetUnsafeValue());
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    WrappedType value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = param_type::FromUnsafeValue(value);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.GetUnsafeValue(), l);
  }
};

}  // namespace IPC

#endif  // GPU_IPC_ID_TYPE_TRAITS_H_
