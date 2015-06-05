// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTILITY_PROCESS_CONTROL_IMPL_H_
#define UTILITY_PROCESS_CONTROL_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/process_control.mojom.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

class GURL;

namespace mojo {
namespace shell {
class ApplicationLoader;
}  // namespace shell
}  // namespace mojo

namespace content {

// Implementation of the ProcessControl interface. Exposed to the browser via
// the utility process's ServiceRegistry.
class UtilityProcessControlImpl : public ProcessControl {
 public:
  UtilityProcessControlImpl();
  ~UtilityProcessControlImpl() override;

  using URLToLoaderMap = std::map<GURL, mojo::shell::ApplicationLoader*>;

  // ProcessControl:
  void LoadApplication(const mojo::String& url,
                       mojo::InterfaceRequest<mojo::Application> request,
                       const LoadApplicationCallback& callback) override;

 private:
  URLToLoaderMap url_to_loader_map_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessControlImpl);
};

}  // namespace content

#endif  // UTILITY_PROCESS_CONTROL_IMPL_H_
