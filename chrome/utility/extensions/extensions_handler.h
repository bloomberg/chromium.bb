// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_
#define CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_

#include "build/build_config.h"
#include "extensions/features/features.h"
#include "extensions/utility/utility_handler.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

namespace extensions {

void InitExtensionsClient();

void PreSandboxStartup();

void ExposeInterfacesToBrowser(service_manager::BinderRegistry* registry,
                               bool running_elevated);

}  // namespace extensions

#endif  // CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_
