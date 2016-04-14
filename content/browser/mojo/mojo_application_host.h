// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_APPLICATION_HOST_H_
#define CONTENT_BROWSER_MOJO_MOJO_APPLICATION_HOST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/application_setup.mojom.h"
#include "content/common/mojo/service_registry_impl.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/service_registry_android.h"
#endif

namespace IPC {
class Sender;
}

namespace content {

// MojoApplicationHost represents the code needed on the browser side to setup
// a child process as a Mojo application. The child process should use the token
// from GetToken() to initialize its MojoApplication. MojoApplicationHost makes
// the ServiceRegistry interface available so that child-provided services can
// be invoked.
class CONTENT_EXPORT MojoApplicationHost {
 public:
  MojoApplicationHost();
  ~MojoApplicationHost();

  // Returns a token to pass to the child process to initialize its
  // MojoApplication.
  const std::string& GetToken() { return token_; }

  ServiceRegistry* service_registry() { return &service_registry_; }

#if defined(OS_ANDROID)
  ServiceRegistryAndroid* service_registry_android() {
    return service_registry_android_.get();
  }
#endif

 private:
  const std::string token_;

  std::unique_ptr<mojom::ApplicationSetup> application_setup_;
  ServiceRegistryImpl service_registry_;

#if defined(OS_ANDROID)
  std::unique_ptr<ServiceRegistryAndroid> service_registry_android_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MojoApplicationHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_APPLICATION_HOST_H_
