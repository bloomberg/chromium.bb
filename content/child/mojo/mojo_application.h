// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_
#define CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_

#include <string>

#include "base/macros.h"
#include "content/common/mojo/service_registry_impl.h"

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

  ServiceRegistry* service_registry() { return &service_registry_; }

 private:
  ServiceRegistryImpl service_registry_;

  DISALLOW_COPY_AND_ASSIGN(MojoApplication);
};

}  // namespace content

#endif  // CONTENT_CHILD_MOJO_MOJO_APPLICATION_H_
