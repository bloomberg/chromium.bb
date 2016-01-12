// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ARC_VIDEO_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ARC_VIDEO_HOST_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace arc {
class VideoHostDelegate;
}

namespace content {

CONTENT_EXPORT scoped_ptr<arc::VideoHostDelegate> CreateArcVideoHostDelegate();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ARC_VIDEO_HOST_DELEGATE_H_
