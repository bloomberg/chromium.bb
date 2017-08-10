// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_FUCHSIA_H_
#define IPC_HANDLE_FUCHSIA_H_

#include <magenta/types.h>

#include <string>

#include "ipc/ipc_export.h"
#include "ipc/ipc_param_traits.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace IPC {

class IPC_EXPORT HandleFuchsia {
 public:
  // Default constructor makes an invalid mx_handle_t.
  HandleFuchsia();
  explicit HandleFuchsia(const mx_handle_t& handle);

  mx_handle_t get_handle() const { return handle_; }
  void set_handle(mx_handle_t handle) { handle_ = handle; }

 private:
  mx_handle_t handle_;
};

template <>
struct IPC_EXPORT ParamTraits<HandleFuchsia> {
  typedef HandleFuchsia param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // IPC_HANDLE_FUCHSIA_H_
