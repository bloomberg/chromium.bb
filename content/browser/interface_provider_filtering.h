// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTERFACE_PROVIDER_FILTERING_H_
#define CONTENT_BROWSER_INTERFACE_PROVIDER_FILTERING_H_

#include "base/strings/string_piece.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

namespace content {

// Filters interface requests received from an execution context of the type
// corresponding to |spec| in the renderer process with ID |process_id|.
// |request| is the InterfaceProviderRequest from the renderer; an equivalent
// InterfaceProviderRequest where GetInterface requests have been filtered.
//
// If |process_id| does not refer to a renderer process or if that renderer's
// BrowserContext does not have a Connector, the connection is broken instead;
// that is, |request| and the InterfacePtr corresponding to the returned request
// are both closed.
service_manager::mojom::InterfaceProviderRequest
FilterRendererExposedInterfaces(
    const char* spec,
    int process_id,
    service_manager::mojom::InterfaceProviderRequest request);

}  // namespace content
#endif  // CONTENT_BROWSER_INTERFACE_PROVIDER_FILTERING_H_
