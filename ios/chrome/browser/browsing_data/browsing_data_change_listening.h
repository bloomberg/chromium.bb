// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CHANGE_LISTENING_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CHANGE_LISTENING_H_

// Listener for changes in browsing data.
@protocol BrowsingDataChangeListening
// Called when cookie storage is changed. Can be called on a background thread.
- (void)didChangeCookieStorage;
@end

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CHANGE_LISTENING_H_
