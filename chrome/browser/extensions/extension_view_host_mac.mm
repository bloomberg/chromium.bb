// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_mac.h"

#import "base/mac/foundation_util.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"

namespace extensions {

ExtensionViewHostMac::~ExtensionViewHostMac() {
  // If there is a popup open for this host's extension, close it.
  ExtensionPopupController* popup = [ExtensionPopupController popup];
  InfoBubbleWindow* window =
      base::mac::ObjCCast<InfoBubbleWindow>([popup window]);
  if ([window isVisible] && [popup extensionViewHost] == this) {
    [window setAllowedAnimations:info_bubble::kAnimateNone];
    [popup close];
  }
}

}  // namespace extensions
