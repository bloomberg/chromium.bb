// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/mus/public/interfaces/gpu.mojom.h"
#include "content/browser/mojo/mojo_shell_client_host.h"

namespace content {

mojo::shell::mojom::CapabilityFilterPtr CreateCapabilityFilterForRenderer() {
  // See https://goo.gl/gkBtCR for a description of what this is and what to
  // think about when changing it.
  mojo::shell::mojom::CapabilityFilterPtr filter(
      mojo::shell::mojom::CapabilityFilter::New());
  mojo::Array<mojo::String> window_manager_interfaces;
  window_manager_interfaces.push_back(mus::mojom::Gpu::Name_);
  filter->filter.insert("mojo:mus", std::move(window_manager_interfaces));
  return filter;
}

}  // namespace content
