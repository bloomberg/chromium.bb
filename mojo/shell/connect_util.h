// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONNECT_UTIL_H_
#define MOJO_SHELL_CONNECT_UTIL_H_

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/shell/public/cpp/identity.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"

namespace mojo {
namespace shell {

class Shell;

ScopedMessagePipeHandle ConnectToInterfaceByName(
    Shell* shell,
    const Identity& source,
    const Identity& target,
    const std::string& interface_name);

// Must only be used by shell internals and test code as it does not forward
// capability filters. Runs |name| with a permissive capability filter.
template <typename Interface>
inline void ConnectToInterface(Shell* shell,
                               const Identity& source,
                               const Identity& target,
                               InterfacePtr<Interface>* ptr) {
  ScopedMessagePipeHandle service_handle =
      ConnectToInterfaceByName(shell, source, target, Interface::Name_);
  ptr->Bind(InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

template <typename Interface>
inline void ConnectToInterface(Shell* shell,
                               const Identity& source,
                               const std::string& name,
                               InterfacePtr<Interface>* ptr) {
  ScopedMessagePipeHandle service_handle = ConnectToInterfaceByName(
      shell, source, Identity(name, mojom::kInheritUserID),
      Interface::Name_);
  ptr->Bind(InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

}  // namespace shell
}  // namespace mojo


#endif  // MOJO_SHELL_CONNECT_UTIL_H_
