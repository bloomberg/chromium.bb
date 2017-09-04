// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/adaptor/url_loader_adaptor.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/referrer.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation URLLoaderAdaptor

@synthesize viewControllerForAlert = _viewControllerForAlert;

#pragma mark - UrlLoader

- (void)loadURL:(const GURL&)url
             referrer:(const web::Referrer&)referrer
           transition:(ui::PageTransition)transition
    rendererInitiated:(BOOL)rendererInitiated {
  // TODO(crbug.com/740793): Remove alert once there is a way to open URL.
  [self showAlert:[NSString
                      stringWithFormat:@"Open URL: %@",
                                       base::SysUTF8ToNSString(url.spec())]];
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
  [self showAlert:[NSString
                      stringWithFormat:@"Open new tab with URL: %@",
                                       base::SysUTF8ToNSString(url.spec())]];
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
               inIncognito:(BOOL)inIncognito
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
  [self showAlert:[NSString
                      stringWithFormat:@"Open new tab with URL: %@",
                                       base::SysUTF8ToNSString(url.spec())]];
}

- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab {
  [self showAlert:@"Load session tab"];
}

- (void)loadJavaScriptFromLocationBar:(NSString*)script {
  [self showAlert:@"Load javascript"];
}

#pragma mark - Private Methods

// TODO(crbug.com/740793): Remove this method once no method is using it.
- (void)showAlert:(NSString*)message {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:message
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewControllerForAlert presentViewController:alertController
                                            animated:YES
                                          completion:nil];
}

@end
