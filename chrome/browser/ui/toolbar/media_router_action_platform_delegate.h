// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_PLATFORM_DELEGATE_H_

#include "base/memory/scoped_ptr.h"

class Browser;

class MediaRouterActionPlatformDelegate {
 public:
  MediaRouterActionPlatformDelegate() {}
  virtual ~MediaRouterActionPlatformDelegate() {}

  // Returns a created MediaRouterActionPlatformDelegate. This is defined in the
  // platform-specific implementation for the class.
  static scoped_ptr<MediaRouterActionPlatformDelegate> Create(Browser* browser);

  // Closes the overflow menu, if it was open. Returns whether or not the
  // overflow menu was closed.
  virtual bool CloseOverflowMenuIfOpen() = 0;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_PLATFORM_DELEGATE_H_
