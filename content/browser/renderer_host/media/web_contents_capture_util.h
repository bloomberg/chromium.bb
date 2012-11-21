// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEB_CONTENTS_CAPTURE_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEB_CONTENTS_CAPTURE_UTIL_H_

#include <string>

namespace content {

class WebContentsCaptureUtil {
 public:
  // Function to extract the target renderer id's from a tab media stream
  // request's device id.
  static bool ExtractTabCaptureTarget(const std::string& device_id,
                                      int* render_process_id,
                                      int* render_view_id);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEB_CONTENTS_CAPTURE_UTIL_H_
