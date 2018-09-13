// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/toolbar/media_router_action_platform_delegate_cocoa.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/base/ui_features.h"

// static
std::unique_ptr<MediaRouterActionPlatformDelegate>
MediaRouterActionPlatformDelegate::CreateCocoa(Browser* browser) {
  return base::WrapUnique(new MediaRouterActionPlatformDelegateCocoa(browser));
}

#if !BUILDFLAG(MAC_VIEWS_BROWSER)
std::unique_ptr<MediaRouterActionPlatformDelegate>
MediaRouterActionPlatformDelegate::Create(Browser* browser) {
  return CreateCocoa(browser);
}
#endif

MediaRouterActionPlatformDelegateCocoa::MediaRouterActionPlatformDelegateCocoa(
    Browser* browser)
    : MediaRouterActionPlatformDelegate(),
      browser_(browser) {
  DCHECK(browser_);
}

MediaRouterActionPlatformDelegateCocoa::
    ~MediaRouterActionPlatformDelegateCocoa() {
}
