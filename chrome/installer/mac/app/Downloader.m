// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "Downloader.h"

#include <assert.h>

@implementation Downloader

@synthesize delegate = delegate_;

+ (NSString*)getChromeDownloadFilePath {
  NSArray* downloadPaths = NSSearchPathForDirectoriesInDomains(
      NSDownloadsDirectory, NSUserDomainMask, YES);
  NSString* filePathToDownloads = [downloadPaths objectAtIndex:0];
  NSArray* filenameComposition = @[ filePathToDownloads, @"GoogleChrome.dmg" ];
  NSString* completeFilePath =
      [NSString pathWithComponents:filenameComposition];
  return completeFilePath;
}

// Downloads contents of chromeURL to downloads folders and delegates the work
// to the DownloadDelegate class.
- (void)downloadChromeImageToDownloadsDirectory:(NSURL*)chromeImageURL {
  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration
                                                 defaultSessionConfiguration]
                                    delegate:self
                               delegateQueue:nil];
  [[session downloadTaskWithURL:chromeImageURL] resume];
  [session finishTasksAndInvalidate];
}

// Skeleton of delegate method to provide download progress updates.
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
  NSURL* downloadsDirectory = [[NSURL alloc]
      initFileURLWithPath:[Downloader getChromeDownloadFilePath]];
  if ([manager fileExistsAtPath:location.path]) {
    [manager moveItemAtURL:location toURL:downloadsDirectory error:nil];
    [delegate_ onDownloadSuccess];
  } else {
  }
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  if (error)
    [delegate_ onDownloadFailureWithError:error];
}

@end
