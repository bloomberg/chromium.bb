// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_LEGACY_CONTEXT_BRIDGE_H_
#define FUCHSIA_ENGINE_LEGACY_CONTEXT_BRIDGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

// Allows chromium::web::Context clients to connect to fuchsia::web::Context
// instances.
//
// LegacyContextBridge instances are self-managed; they destroy themselves when
// the connection with either end is terminated.
class WEB_ENGINE_EXPORT LegacyContextBridge : public chromium::web::Context {
 public:
  LegacyContextBridge(fidl::InterfaceRequest<chromium::web::Context> request,
                      fuchsia::web::ContextPtr handle);

 private:
  // Non-public to ensure that only this object may destroy itself.
  ~LegacyContextBridge() override;

  // chromium::web::Context implementation.
  void CreateFrame(fidl::InterfaceRequest<chromium::web::Frame> frame) override;

  fidl::Binding<chromium::web::Context> binding_;
  fuchsia::web::ContextPtr context_;

  DISALLOW_COPY_AND_ASSIGN(LegacyContextBridge);
};

#endif  // FUCHSIA_ENGINE_LEGACY_CONTEXT_BRIDGE_H_
