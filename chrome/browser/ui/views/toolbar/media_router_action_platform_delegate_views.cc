// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/media_router_action_platform_delegate_views.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views_mode_controller.h"

// static
std::unique_ptr<MediaRouterActionPlatformDelegate>
MediaRouterActionPlatformDelegate::Create(Browser* browser) {
#if defined(OS_MACOSX)
  if (views_mode_controller::IsViewsBrowserCocoa())
    return CreateCocoa(browser);
#endif
  return base::WrapUnique(new MediaRouterActionPlatformDelegateViews(browser));
}

MediaRouterActionPlatformDelegateViews::MediaRouterActionPlatformDelegateViews(
    Browser* browser)
    : MediaRouterActionPlatformDelegate(),
      browser_(browser) {
  DCHECK(browser_);
}

MediaRouterActionPlatformDelegateViews::
    ~MediaRouterActionPlatformDelegateViews() {
}
