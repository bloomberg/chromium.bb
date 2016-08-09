// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "MainDelegate.h"

@implementation MainDelegate

- (void)runApplication {
  OmahaCommunication* omahaMessenger = [[OmahaCommunication alloc] init];
  omahaMessenger.delegate = self;

  [omahaMessenger fetchDownloadURLs];
}

- (void)onOmahaSuccessWithURLs:(NSArray*)URLs {
  Downloader* download = [[Downloader alloc] init];
  download.delegate = self;

  [download downloadChromeImageToDownloadsDirectory:[URLs firstObject]];
}

- (void)onOmahaFailureWithError:(NSError*)error {
  NSLog(@"error: %@", [error localizedDescription]);
  exit(1);
}

- (void)onDownloadSuccess {
  NSLog(@"end of program, exiting.\nin the ideal world, we would be unpacking "
        @"now");
  exit(0);
}

- (void)onDownloadFailureWithError:(NSError*)error {
  NSLog(@"error: %@", [error localizedDescription]);
  exit(1);
}

@end
