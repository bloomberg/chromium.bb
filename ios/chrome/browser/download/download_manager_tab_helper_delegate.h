// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class DownloadManagerTabHelper;
namespace web {
class DownloadTask;
}  // namespace web

// Delegate for DownloadManagerTabHelper class.
@protocol DownloadManagerTabHelperDelegate<NSObject>

// Informs the delegate that a DownloadTask was created.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download;

// Informs the delegate that a DownloadTask was updated (download progress was
// changed, or download has reached a terminal state).
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didUpdateDownload:(nonnull web::DownloadTask*)download;

// Informs the delegate that WebState related to this download was hidden.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(nonnull web::DownloadTask*)download;

// Informs the delegate that WebState related to this download was shown.
- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(nonnull web::DownloadTask*)download;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_
