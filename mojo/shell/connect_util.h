// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONNECT_UTIL_H_
#define MOJO_SHELL_CONNECT_UTIL_H_

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace mojo {
namespace shell {

class ApplicationManager;

ScopedMessagePipeHandle ConnectToInterfaceByName(
    ApplicationManager* application_manager,
    const Identity& source,
    const Identity& target,
    const std::string& interface_name);

// Must only be used by shell internals and test code as it does not forward
// capability filters. Runs |application_name| with a permissive capability
// filter.
template <typename Interface>
inline void ConnectToInterface(ApplicationManager* application_manager,
                               const Identity& source,
                               const Identity& target,
                               InterfacePtr<Interface>* ptr) {
  ScopedMessagePipeHandle service_handle = ConnectToInterfaceByName(
      application_manager, source, target, Interface::Name_);
  ptr->Bind(InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

template <typename Interface>
inline void ConnectToInterface(ApplicationManager* application_manager,
                               const Identity& source,
                               const std::string& application_name,
                               InterfacePtr<Interface>* ptr) {
  ScopedMessagePipeHandle service_handle = ConnectToInterfaceByName(
      application_manager, source,
      Identity(application_name, std::string(), mojom::Connector::kUserInherit),
      Interface::Name_);
  ptr->Bind(InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

}  // namespace shell
}  // namespace mojo


#endif  // MOJO_SHELL_CONNECT_UTIL_H_
