// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_DOWNLOADER_H_
#define CHROME_INSTALLER_MAC_APP_DOWNLOADER_H_

#import <Foundation/Foundation.h>

@protocol DownloaderDelegate
- (void)onDownloadSuccess;
- (void)onDownloadFailureWithError:(NSError*)error;
@end

@interface Downloader
    : NSObject<NSXMLParserDelegate, NSURLSessionDownloadDelegate>

@property(nonatomic, assign) id<DownloaderDelegate> delegate;

// Takes an NSData with a response XML from Omaha and writes the latest
// version of chrome to the user's download directory.
- (void)downloadChromeImageToDownloadsDirectory:(NSURL*)chromeImageURL;

// Returns a path to a user's home download folder.
+ (NSString*)getChromeDownloadFilePath;

@end

#endif  // CHROME_INSTALLER_MAC_APP_DOWNLOADER_H_
