// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SHELL_CONNECT_H_
#define MOJO_PUBLIC_CPP_SHELL_CONNECT_H_

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"

namespace mojo {

template <typename Interface>
inline void ConnectTo(Shell* shell, const std::string& url,
                      InterfacePtr<Interface>* ptr) {
  MessagePipe pipe;
  ptr->Bind(pipe.handle0.Pass());

  AllocationScope scope;
  shell->Connect(url, pipe.handle1.Pass());
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SHELL_CONNECT_H_
