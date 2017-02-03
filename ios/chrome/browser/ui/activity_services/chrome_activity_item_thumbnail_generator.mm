// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace activity_services {

ThumbnailGeneratorBlock ThumbnailGeneratorForTab(Tab* tab) {
  DCHECK(tab);
  // Do not generate thumbnails for incognito tabs.
  if (tab.browserState->IsOffTheRecord()) {
    return ^UIImage*(CGSize const& size) { return nil; };
  } else {
    __weak Tab* weakTab = tab;
    return ^UIImage*(CGSize const& size) {
      Tab* strongTab = weakTab;
      UIImage* snapshot =
          [strongTab generateSnapshotWithOverlay:NO visibleFrameOnly:YES];
      UIImage* thumbnail =
          ResizeImage(snapshot, size, ProjectionMode::kAspectFillAlignTop, YES);
      return thumbnail;
    };
  }
}

}  // namespace activity_services
