// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PREVIEWS_STATE_HELPER_H_
#define CONTENT_RENDERER_PREVIEWS_STATE_HELPER_H_

#include "content/public/common/previews_state.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace content {

// Determines and returns an updated PreviewsState value for a main frame
// based on its |original_state| value and its |web_url_response|.
CONTENT_EXPORT PreviewsState GetPreviewsStateFromMainFrameResponse(
    PreviewsState original_state,
    const blink::WebURLResponse& web_url_response);

}  // namespace content

#endif  // CONTENT_RENDERER_PREVIEWS_STATE_HELPER_H_
