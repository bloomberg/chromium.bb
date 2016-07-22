// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See header file for description of RendererPreconnect class

#include "components/network_hints/renderer/renderer_preconnect.h"

#include "components/network_hints/common/network_hints_common.h"
#include "components/network_hints/public/cpp/network_hints_param_traits.h"
#include "content/public/renderer/render_thread.h"
#include "services/shell/public/cpp/interface_provider.h"

using content::RenderThread;

namespace network_hints {

RendererPreconnect::RendererPreconnect() {
}

RendererPreconnect::~RendererPreconnect() {
}

void RendererPreconnect::Preconnect(const GURL& url, bool allow_credentials) {
  if (!url.is_valid())
    return;

  GetNetworkHints().Preconnect(url, allow_credentials, 1);
}

mojom::NetworkHints& RendererPreconnect::GetNetworkHints() {
  DCHECK(content::RenderThread::Get());
  if (!network_hints_) {
    RenderThread::Get()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&network_hints_));
  }
  return *network_hints_;
}

}  // namespace network_hints
