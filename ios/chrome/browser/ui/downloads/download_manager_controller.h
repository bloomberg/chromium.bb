// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOADS_DOWNLOAD_MANAGER_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOADS_DOWNLOAD_MANAGER_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/native_content_controller.h"

@protocol StoreKitLauncher;

namespace net {
class URLRequestContextGetter;
}

// This controller shows the native download manager. It shows the user basic
// information about the file (namely its type, size, and name), and gives them
// the option to download it and open it in another app. This controller is
// displayed when a URL is loaded that contains a file type that UIWebView
// cannot display itself.
@interface DownloadManagerController
    : NativeContentController<UIDocumentInteractionControllerDelegate>

// Initializes a controller for content from |url|, using
// |requestContextGetter| for all requests necessary to download the file.
// |storeLauncher| is used to open a controller that allows the user to
// install Google Drive if they don't have it installed.
- (id)initWithURL:(const GURL&)url
    requestContextGetter:(net::URLRequestContextGetter*)requestContextGetter
        storeKitLauncher:(id<StoreKitLauncher>)storeLauncher;

// Starts loading the data for the file at the url passed into the
// initializer. This should only be called once, immediately after
// initialization.
- (void)start;

// Deletes the directory in which downloaded files are saved. It should only be
// called on the UI thread.
+ (void)clearDownloadsDirectory;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOADS_DOWNLOAD_MANAGER_CONTROLLER_H_
