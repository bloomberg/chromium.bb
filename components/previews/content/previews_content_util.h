// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_CONTENT_UTIL_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_CONTENT_UTIL_H_

#include "components/previews/core/previews_decider.h"
#include "content/public/common/previews_state.h"

namespace previews {

// Returns the bitmask of enabled client-side previews for |url_request| and
// the current effective network connection given |previews_decider|.
// This handles the mapping of previews::PreviewsType enum values to bitmask
// definitions for content::PreviewsState.
content::PreviewsState DetermineClientPreviewsState(
    const net::URLRequest& url_request,
    previews::PreviewsDecider* previews_decider);

// Returns the effective PreviewsType known on a main frame basis given the
// |previews_state| bitmask for the committed main frame. Will return NONE
// for LoFi since it would be determined at image subresource load time.
previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state);

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_CONTENT_UTIL_H_
