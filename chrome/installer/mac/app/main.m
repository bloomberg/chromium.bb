// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "Downloader.h"
#import "OmahaCommunication.h"
#import "OmahaXMLRequest.h"
#import "SystemInfo.h"

// TODO: add a class that takes care of what main is doing now
void talkToOmahaThenExecuteBlock(OmahaRequestCompletionHandler block) {
  NSXMLDocument* requestBody = [OmahaXMLRequest createXMLRequestBody];
  OmahaCommunication* messenger =
      [[OmahaCommunication alloc] initWithBody:requestBody];
  [messenger sendRequestWithBlock:block];
}

int main() {
  talkToOmahaThenExecuteBlock(^(NSData* data, NSError* error) {
    if (error) {
      NSLog(@"%@", [error localizedDescription]);
      return;
    }
    Downloader* download = [[Downloader alloc] init];
    [download downloadChromeImageToDownloadsDirectory:data];
  });

  // [[NSRunLoop mainRunLoop] run];
  [[NSRunLoop mainRunLoop]
      runUntilDate:[NSDate dateWithTimeIntervalSinceNow:3]];
  return 1;
}
