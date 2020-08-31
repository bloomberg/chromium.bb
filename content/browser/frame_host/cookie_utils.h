// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_COOKIE_UTILS_H_
#define CONTENT_BROWSER_FRAME_HOST_COOKIE_UTILS_H_

#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace content {

class RenderFrameHostImpl;
struct CookieAccessDetails;

void SplitCookiesIntoAllowedAndBlocked(
    const network::mojom::CookieAccessDetailsPtr& cookie_details,
    CookieAccessDetails* allowed,
    CookieAccessDetails* blocked);

// TODO(crbug.com/977040): Remove when no longer needed.
void EmitSameSiteCookiesDeprecationWarning(
    RenderFrameHostImpl* rfh,
    const network::mojom::CookieAccessDetailsPtr& cookie_details);

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_COOKIE_UTILS_H_
