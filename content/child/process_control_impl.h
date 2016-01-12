// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PROCESS_CONTROL_IMPL_H_
#define CONTENT_CHILD_PROCESS_CONTROL_IMPL_H_

#include <map>

#include "base/macros.h"
#include "content/common/process_control.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/interfaces/application.mojom.h"

class GURL;

namespace mojo {
namespace shell {
class ApplicationLoader;
}  // namespace shell
}  // namespace mojo

namespace content {

// Default implementation of the ProcessControl interface.
class ProcessControlImpl : public ProcessControl {
 public:
  ProcessControlImpl();
  ~ProcessControlImpl() override;

  using URLToLoaderMap = std::map<GURL, mojo::shell::ApplicationLoader*>;

  // Registers Mojo applications loaders for URLs.
  virtual void RegisterApplicationLoaders(
      URLToLoaderMap* url_to_loader_map) = 0;

  // ProcessControl:
  void LoadApplication(const mojo::String& url,
                       mojo::InterfaceRequest<mojo::Application> request,
                       const LoadApplicationCallback& callback) override;

 private:
  // Called if a LoadApplication request fails.
  virtual void OnLoadFailed() {}

  bool has_registered_loaders_ = false;
  URLToLoaderMap url_to_loader_map_;

  DISALLOW_COPY_AND_ASSIGN(ProcessControlImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_PROCESS_CONTROL_IMPL_H_
