// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PROCESS_CONTROL_IMPL_H_
#define CONTENT_CHILD_PROCESS_CONTROL_IMPL_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "content/common/mojo/embedded_application_runner.h"
#include "content/common/process_control.mojom.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace content {

// Default implementation of the mojom::ProcessControl interface.
class ProcessControlImpl : public mojom::ProcessControl {
 public:
  using ApplicationFactoryMap =
      std::unordered_map<std::string,
                         EmbeddedApplicationRunner::FactoryCallback>;

  ProcessControlImpl();
  ~ProcessControlImpl() override;

  virtual void RegisterApplicationFactories(
      ApplicationFactoryMap* factories) = 0;
  virtual void OnApplicationQuit() {}

  // ProcessControl:
  void LoadApplication(const mojo::String& name,
                       shell::mojom::ShellClientRequest request,
                       const LoadApplicationCallback& callback) override;

 private:
  // Called if a LoadApplication request fails.
  virtual void OnLoadFailed() {}

  bool has_registered_apps_ = false;
  std::unordered_map<std::string, std::unique_ptr<EmbeddedApplicationRunner>>
      apps_;

  DISALLOW_COPY_AND_ASSIGN(ProcessControlImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_PROCESS_CONTROL_IMPL_H_
