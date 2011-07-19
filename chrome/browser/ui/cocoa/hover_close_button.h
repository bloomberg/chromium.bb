// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/hover_button.h"

#include "base/mac/cocoa_protocols.h"

@class GTMKeyValueAnimation;

// The standard close button for our Mac UI which is the "x" that changes to a
// dark circle with the "x" when you hover over it. At this time it is used by
// the download bar, info bar and tabs.
@interface HoverCloseButton : HoverButton<NSAnimationDelegate> {
 @private
  GTMKeyValueAnimation* fadeOutAnimation_;
  HoverState previousState_;
}

@end
