// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <assert.h>

#import "DownloadDelegate.h"
#import "Downloader.h"

@implementation DownloadDelegate : NSObject

// Skeleton of delegate method to provide download progress updates.
// TODO: Make use of (totalBytesWritten/totalBytesExpectedToWrite)*100
// to generate download progress percentage.
- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
}

// Delegate method to move downloaded disk image to user's Download directory.
- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  assert([location isFileURL]);
  NSFileManager* manager = [[NSFileManager alloc] init];
  NSURL* downloadsDirectory =
      [[NSURL alloc] initFileURLWithPath:[Downloader getDownloadsFilePath]];
  if ([manager fileExistsAtPath:location.path]) {
    [manager copyItemAtURL:location toURL:downloadsDirectory error:nil];
  } else {
    // TODO: Error Handling
  }
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  // TODO: Error Handling
}
@end
