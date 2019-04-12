// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_LEGACY_CONTEXT_PROVIDER_BRIDGE_H_
#define FUCHSIA_ENGINE_LEGACY_CONTEXT_PROVIDER_BRIDGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

// Allows chromium::web::ContextProvider clients to connect to
// fuchsia::web::ContextProvider instances.
class WEB_ENGINE_EXPORT LegacyContextProviderBridge
    : public chromium::web::ContextProvider {
 public:
  LegacyContextProviderBridge(fuchsia::web::ContextProviderPtr handle);
  ~LegacyContextProviderBridge() override;

 private:
  // chromium::web::ContextProvider implementation.
  void Create(
      chromium::web::CreateContextParams params,
      fidl::InterfaceRequest<chromium::web::Context> context_request) override;

  fuchsia::web::ContextProviderPtr context_provider_;

  DISALLOW_COPY_AND_ASSIGN(LegacyContextProviderBridge);
};

#endif  // FUCHSIA_ENGINE_LEGACY_CONTEXT_PROVIDER_BRIDGE_H_
