// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

struct UrlLoadParams;

@class OpenNewTabCommand;

// Objective-C delegate for AppUrlLoadingService.
@protocol AppURLLoadingServiceDelegate

// Implementing delegate must open the url in |command| in a new tab.
// TODO(crbug.com/907527): split into simpler actions, like dismissModals,
// switchModeIfneeded, etc.
- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command;

@end

// Service used to manage url loading at application level.
class AppUrlLoadingService {
 public:
  AppUrlLoadingService();

  void SetDelegate(id<AppURLLoadingServiceDelegate> delegate);

  // Opens a url based on |params| in a new tab.
  virtual void LoadUrlInNewTab(UrlLoadParams* params);

 private:
  __weak id<AppURLLoadingServiceDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_
