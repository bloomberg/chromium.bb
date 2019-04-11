// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/legacy_context_provider_bridge.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/engine/legacy_context_bridge.h"

LegacyContextProviderBridge::LegacyContextProviderBridge(
    fuchsia::web::ContextProviderPtr handle)
    : context_provider_(std::move(handle)) {}

LegacyContextProviderBridge::~LegacyContextProviderBridge() = default;

void LegacyContextProviderBridge::Create(
    chromium::web::CreateContextParams params,
    fidl::InterfaceRequest<chromium::web::Context> context_request) {
  fuchsia::web::CreateContextParams fuchsia_params;
  if (params.has_service_directory()) {
    fuchsia_params.set_service_directory(
        std::move(*params.mutable_service_directory()));
  }

  if (params.has_data_directory()) {
    fuchsia_params.set_data_directory(
        std::move(*params.mutable_data_directory()));
  }

  fuchsia::web::ContextPtr fuchsia_context;
  context_provider_->Create(std::move(fuchsia_params),
                            fuchsia_context.NewRequest());
  new LegacyContextBridge(std::move(context_request),
                          std::move(fuchsia_context));
}
