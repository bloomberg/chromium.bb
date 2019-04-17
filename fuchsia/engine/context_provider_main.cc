// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_main.h"

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "fuchsia/base/lifecycle_impl.h"
#include "fuchsia/engine/context_provider_impl.h"

int ContextProviderMain() {
  base::MessageLoopForUI message_loop;
  base::fuchsia::ServiceDirectory* const directory =
      base::fuchsia::ServiceDirectory::GetDefault();

  ContextProviderImpl context_provider;
  base::fuchsia::ScopedServiceBinding<fuchsia::web::ContextProvider> binding(
      directory, &context_provider);

  fuchsia::web::ContextProviderPtr fuchsia_context_provider;
  fidl::Binding<fuchsia::web::ContextProvider> fuchsia_binding(
      &context_provider, fuchsia_context_provider.NewRequest());

  base::RunLoop run_loop;
  cr_fuchsia::LifecycleImpl lifecycle(directory, run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
