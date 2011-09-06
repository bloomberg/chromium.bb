// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_browser_window.h"

#include "base/logging.h"

@implementation ChromeBrowserWindow

- (void)underlaySurfaceAdded {
  DCHECK_GE(underlaySurfaceCount_, 0);
  ++underlaySurfaceCount_;

  // We're having the OpenGL surface render under the window, so the window
  // needs to be not opaque.
  if (underlaySurfaceCount_ == 1)
    [self setOpaque:NO];
}

- (void)underlaySurfaceRemoved {
  --underlaySurfaceCount_;
  DCHECK_GE(underlaySurfaceCount_, 0);

  if (underlaySurfaceCount_ == 0)
    [self setOpaque:YES];
}

@end
