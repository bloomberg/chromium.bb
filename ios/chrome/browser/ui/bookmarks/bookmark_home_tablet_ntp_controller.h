// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_TABLET_NTP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_TABLET_NTP_CONTROLLER_H_

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"

// Navigate/edit the bookmark hierarchy on tablet, from the New Tab Page (NTP).
@interface BookmarkHomeTabletNTPController
    : BookmarkHomeViewController<NewTabPagePanelProtocol>

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_TABLET_NTP_CONTROLLER_H_
