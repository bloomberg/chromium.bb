// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/mojo/mojo_shell_client_host.h"

#if defined(MOJO_SHELL_CLIENT)
#include "components/mus/public/interfaces/gpu.mojom.h"
#endif

namespace content {

mojo::shell::mojom::CapabilityFilterPtr CreateCapabilityFilterForRenderer() {
  // See https://goo.gl/gkBtCR for a description of what this is and what to
  // think about when changing it.
  mojo::shell::mojom::CapabilityFilterPtr filter(
      mojo::shell::mojom::CapabilityFilter::New());
  filter->filter.SetToEmpty();
#if defined(MOJO_SHELL_CLIENT)
  mojo::Array<mojo::String> window_manager_interfaces;
  window_manager_interfaces.push_back(mus::mojom::Gpu::Name_);
  filter->filter.insert("mojo:mus", std::move(window_manager_interfaces));
#endif
  return filter;
}

}  // namespace content
