// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_MEDIA_CAPTURE_UTIL_H_
#define EXTENSIONS_SHELL_BROWSER_MEDIA_CAPTURE_UTIL_H_

#include "base/macros.h"
#include "content/public/common/media_stream_request.h"

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

namespace media_capture_util {

// Grants access to the first available audio capture device (if requested) and
// the first available video capture device (if requested). Usually used as a
// helper for media capture ProcessMediaAccessRequest().
void GrantMediaStreamRequestWithFirstDevice(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const Extension* extension);

}  // namespace media_capture_util
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_MEDIA_CAPTURE_UTIL_H_
