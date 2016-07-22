// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_APP_DOWNLOADER_H_
#define CHROME_INSTALLER_MAC_APP_DOWNLOADER_H_

#import <Foundation/Foundation.h>

@protocol DownloaderDelegate
- (void)onDownloadSuccess;
@end

@interface Downloader
    : NSObject<NSXMLParserDelegate, NSURLSessionDownloadDelegate>

@property(nonatomic, assign) id<DownloaderDelegate> delegate;

// Returns a path to a user's home download folder.
+ (NSString*)getDownloadsFilePath;

- (NSMutableArray*)appendFilename:(NSString*)filename
                           toURLs:(NSArray*)incompleteURLs;

// Takes an NSData with a response XML from Omaha and writes the latest
// version of chrome to the user's download directory.
- (BOOL)downloadChromeImageToDownloadsDirectory:(NSData*)omahaResponseXML;

@end

#endif  // CHROME_INSTALLER_MAC_APP_DOWNLOADER_H_
