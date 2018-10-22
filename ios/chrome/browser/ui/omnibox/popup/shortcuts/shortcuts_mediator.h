// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

namespace favicon {
class LargeIconService;
}
namespace ntp_tiles {
class MostVisitedSites;
}
class LargeIconCache;
class ReadingListModel;

@protocol ShortcutsConsumer;

// Coordinator for the Omnibox Popup Shortcuts.
@interface ShortcutsMediator : NSObject

- (instancetype)
initWithLargeIconService:(favicon::LargeIconService*)largeIconService
          largeIconCache:(LargeIconCache*)largeIconCache
         mostVisitedSite:
             (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
        readingListModel:(ReadingListModel*)readingListModel;

@property(nonatomic, weak) id<ShortcutsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_MEDIATOR_H_
