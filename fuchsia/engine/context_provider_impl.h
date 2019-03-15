// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_
#define FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_

#include <lib/fidl/cpp/binding_set.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chromium/web/cpp/fidl.h"
#include "fuchsia/engine/web_engine_export.h"

namespace base {
class CommandLine;
struct LaunchOptions;
class Process;
}  // namespace base

class WEB_ENGINE_EXPORT ContextProviderImpl
    : public chromium::web::ContextProvider {
 public:
  using LaunchCallbackForTest = base::RepeatingCallback<base::Process(
      const base::CommandLine& command,
      const base::LaunchOptions& options)>;

  ContextProviderImpl();
  ~ContextProviderImpl() override;

  // Binds |this| object instance to |request|.
  // The service will persist and continue to serve other channels in the event
  // that a bound channel is dropped.
  void Bind(fidl::InterfaceRequest<chromium::web::ContextProvider> request);

  // chromium::web::ContextProvider implementation.
  void Create(chromium::web::CreateContextParams params,
              ::fidl::InterfaceRequest<chromium::web::Context> context_request)
      override;

  // Sets a |launch| callback to use instead of calling LaunchProcess() to
  // create Context processes.
  void SetLaunchCallbackForTest(LaunchCallbackForTest launch);

 private:
  // Set by tests to use to launch Context child processes, e.g. to allow a
  // fake Context process to be launched.
  LaunchCallbackForTest launch_for_test_;

  fidl::BindingSet<chromium::web::ContextProvider> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderImpl);
};

#endif  // FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_
