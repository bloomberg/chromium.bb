// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/onscreen_display_client.h"

#include "cc/output/output_surface.h"

namespace content {

OnscreenDisplayClient::OnscreenDisplayClient(
    const scoped_refptr<cc::ContextProvider>& onscreen_context_provider,
    cc::SurfaceManager* manager)
    : onscreen_context_provider_(onscreen_context_provider),
      display_(this, manager) {
}

OnscreenDisplayClient::~OnscreenDisplayClient() {
}

scoped_ptr<cc::OutputSurface> OnscreenDisplayClient::CreateOutputSurface() {
  return make_scoped_ptr(new cc::OutputSurface(onscreen_context_provider_))
      .Pass();
}

}  // namespace content
