// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_
#define CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_

#include <string>

#include "base/macros.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace content {

// MojoApplication represents the code needed to setup a child process as a
// Mojo application. Instantiate MojoApplication and call InitWithToken() with
// a token passed from the process host. It makes the ServiceRegistry interface
// available.
class MojoApplication {
 public:
  MojoApplication();
  virtual ~MojoApplication();

  // Initializes this MojoApplicaiton with a message pipe obtained using
  // |token|.
  void InitWithToken(const std::string& token);

  shell::InterfaceRegistry* interface_registry() {
    return interface_registry_.get();
  }
  shell::InterfaceProvider* remote_interfaces() {
    return remote_interfaces_.get();
  }

 private:
  std::unique_ptr<shell::InterfaceRegistry> interface_registry_;
  std::unique_ptr<shell::InterfaceProvider> remote_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(MojoApplication);
};

}  // namespace content

#endif  // CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_
