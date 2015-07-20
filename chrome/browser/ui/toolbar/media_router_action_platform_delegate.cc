// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/toolbar/media_router_action_platform_delegate.h"

#if defined(OS_MACOSX)
// static
scoped_ptr<MediaRouterActionPlatformDelegate>
MediaRouterActionPlatformDelegate::Create(Browser* browser) {
  return nullptr;
}
#endif

MediaRouterActionPlatformDelegate::MediaRouterActionPlatformDelegate() {
}

#if defined(OS_MACOSX)
MediaRouterActionPlatformDelegate::MediaRouterActionPlatformDelegate(
    Browser* browser) {
  NOTIMPLEMENTED();
}

void MediaRouterActionPlatformDelegate::CloseOverflowMenuIfOpen() {
  NOTIMPLEMENTED();
}
#endif
