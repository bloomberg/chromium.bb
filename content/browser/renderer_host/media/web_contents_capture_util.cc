// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/web_contents_capture_util.h"

#include "base/string_number_conversions.h"
#include "base/string_piece.h"

namespace content {

bool WebContentsCaptureUtil::ExtractTabCaptureTarget(
    const std::string& device_id,
    int* render_process_id,
    int* render_view_id) {
  const size_t sep_pos = device_id.find(':');
  if (sep_pos == std::string::npos)
    return false;

  const base::StringPiece component1(device_id.data(), sep_pos);
  const base::StringPiece component2(device_id.data() + sep_pos + 1,
                                     device_id.length() - sep_pos - 1);

  return (base::StringToInt(component1, render_process_id) &&
          base::StringToInt(component2, render_view_id));
}

}  // namespace content
