// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_H_
#define CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// Class that bridges BrowserAccessibilityManager and platform-dependent
// handler.
// TODO(crbug.com/727210): Expand this class to work on all the platforms.
class CONTENT_EXPORT WebContentsAccessibility {
 public:
  WebContentsAccessibility() {}
  virtual ~WebContentsAccessibility() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsAccessibility);
};
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_H_
