// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_WEB_ACTIVITY_HELPERS_H_
#define ATHENA_CONTENT_WEB_ACTIVITY_HELPERS_H_

namespace content {
class WebContents;
}

namespace athena {

// Called right after |contents| is associated with a WebActivity to attach
// helper classes to |contents|. This method should not set the visibility,
// activation or bounds of |contents|.
void AttachWebActivityHelpers(content::WebContents* contents);

}  // namespace athena

#endif  // ATHENA_CONTENT_WEB_ACTIVITY_HELPERS_H_
