// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_TABLET_NTP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_TABLET_NTP_CONTROLLER_H_

#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"

@protocol UrlLoader;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Navigate/edit the bookmark hierarchy on tablet, from the New Tab Page (NTP).
@interface BookmarkHomeTabletNTPController : NSObject<NewTabPagePanelProtocol>
// Designated initializer.
- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_TABLET_NTP_CONTROLLER_H_
