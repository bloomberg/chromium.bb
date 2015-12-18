// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONNECT_UTIL_H_
#define MOJO_SHELL_CONNECT_UTIL_H_

#include <utility>

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/handle.h"

class GURL;

namespace mojo {
namespace shell {

class ApplicationManager;

ScopedMessagePipeHandle ConnectToServiceByName(
    ApplicationManager* application_manager,
    const GURL& application_url,
    const std::string& interface_name);

// Must only be used by shell internals and test code as it does not forward
// capability filters. Runs |application_url| with a permissive capability
// filter.
template <typename Interface>
inline void ConnectToService(ApplicationManager* application_manager,
                             const GURL& application_url,
                             InterfacePtr<Interface>* ptr) {
  ScopedMessagePipeHandle service_handle =
      ConnectToServiceByName(application_manager, application_url,
                             Interface::Name_);
  ptr->Bind(InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

}  // namespace shell
}  // namespace mojo


#endif  // MOJO_SHELL_CONNECT_UTIL_H_
