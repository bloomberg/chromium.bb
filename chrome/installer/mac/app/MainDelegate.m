// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "MainDelegate.h"

@implementation MainDelegate

- (void)runApplication {
  OmahaCommunication* messenger = [[OmahaCommunication alloc] init];
  messenger.delegate = self;

  [messenger sendRequest];
}

- (void)onOmahaSuccessWithResponseBody:(NSData*)responseBody
                              AndError:(NSError*)error {
  if (error) {
    NSLog(@"error: %@", [error localizedDescription]);
    exit(1);
  }
  Downloader* download = [[Downloader alloc] init];
  download.delegate = self;

  [download downloadChromeImageToDownloadsDirectory:responseBody];
}

- (void)onDownloadSuccess {
  // TODO: replace the line of code below with real code someday to unpack dmg
  exit(0);
}

- (void)onUnpackSuccess {
}

@end
