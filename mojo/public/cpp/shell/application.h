// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SHELL_APPLICATION_H_
#define MOJO_PUBLIC_SHELL_APPLICATION_H_

#include <vector>

#include "mojo/public/cpp/shell/service.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"

namespace mojo {

class Application : public internal::ServiceConnectorBase::Owner {
 public:
  explicit Application(ScopedMessagePipeHandle shell_handle);
  explicit Application(MojoHandle shell_handle);
  virtual ~Application();

  // internal::ServiceConnectorBase::Owner methods.
  // Takes ownership of |service_connector|.
  virtual void AddServiceConnector(
      internal::ServiceConnectorBase* service_connector) MOJO_OVERRIDE;
  virtual void RemoveServiceConnector(
      internal::ServiceConnectorBase* service_connector) MOJO_OVERRIDE;

  template <typename Interface>
  void ConnectTo(const std::string& url, InterfacePtr<Interface>* ptr) {
    mojo::ConnectTo(shell(), url, ptr);
  }

 protected:
  // ShellClient methods.
  virtual void AcceptConnection(const mojo::String& url,
                                ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE;

 private:
  typedef std::vector<internal::ServiceConnectorBase*> ServiceConnectorList;
  ServiceConnectorList service_connectors_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SHELL_APPLICATION_H_
