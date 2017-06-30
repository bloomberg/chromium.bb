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
  enum Permissions {
    // A placeholder value to be used by the receiving IPC channel, since the
    // permissions information is only used by the broker process.
    INVALID,
    // The new mx_handle_t will have the same permissions as the old
    // mx_handle_t.
    DUPLICATE,
    // The new mx_handle_t will have file read and write permissions.
    FILE_READ_WRITE,
    MAX_PERMISSIONS = FILE_READ_WRITE
  };

  // Default constructor makes an invalid mx_handle_t.
  HandleFuchsia();
  HandleFuchsia(const mx_handle_t& handle, Permissions permissions);

  mx_handle_t get_handle() const { return handle_; }
  void set_handle(mx_handle_t handle) { handle_ = handle; }
  Permissions get_permissions() const { return permissions_; }

 private:
  mx_handle_t handle_;
  Permissions permissions_;
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
