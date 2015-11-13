// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_IPC_MOJO_PARAM_TRAITS_H_
#define IPC_MOJO_IPC_MOJO_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_export.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {
class PickleIterator;
}

namespace IPC {

class Message;

template <>
struct IPC_MOJO_EXPORT ParamTraits<mojo::MessagePipeHandle> {
  typedef mojo::MessagePipeHandle param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // IPC_MOJO_IPC_MOJO_PARAM_TRAITS_H_
