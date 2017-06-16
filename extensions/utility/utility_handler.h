// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_UTILITY_UTILITY_HANDLER_H_
#define EXTENSIONS_UTILITY_UTILITY_HANDLER_H_

#include "services/service_manager/public/cpp/binder_registry.h"

namespace extensions {

namespace utility_handler {

void UtilityThreadStarted();

void ExposeInterfacesToBrowser(service_manager::BinderRegistry* registry,
                               bool running_elevated);

}  // namespace utility_handler

}  // namespace extensions

#endif  // EXTENSIONS_UTILITY_UTILITY_HANDLER_H_
