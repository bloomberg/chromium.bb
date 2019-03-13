// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/ui/chrome_load_params.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/web/public/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class AppUrlLoadingService;
class Browser;
class UrlLoadingNotifier;
struct UrlLoadParams;

@class OpenNewTabCommand;

// Objective-C delegate for UrlLoadingService.
@protocol URLLoadingServiceDelegate

// Implementing delegate can do an animation using information in |params| when
// opening a background tab, then call |completion|.
- (void)animateOpenBackgroundTabFromParams:(UrlLoadParams*)params
                                completion:(void (^)())completion;

@end

// Service used to load url in current or new tab.
class UrlLoadingService : public KeyedService {
 public:
  UrlLoadingService(UrlLoadingNotifier* notifier);

  void SetAppService(AppUrlLoadingService* app_service);
  void SetDelegate(id<URLLoadingServiceDelegate> delegate);
  void SetBrowser(Browser* browser);

  id<UrlLoader> GetUrlLoader();

  // Opens a url based on |chrome_params|.
  // TODO(crbug.com/907527): to be deprecated, use OpenUrl.
  void LoadUrlInCurrentTab(const ChromeLoadParams& chrome_params);

  // Opens a url based on |command| in a new tab.
  // TODO(crbug.com/907527): to be deprecated, use OpenUrl.
  void LoadUrlInNewTab(OpenNewTabCommand* command);

  // Opens a url depending on |params.disposition|.
  void OpenUrl(UrlLoadParams* params);

 private:
  // Switches to a tab that matches |params.web_params| or opens in a new tab.
  virtual void SwitchToTab(UrlLoadParams* params);

  virtual void LoadUrlInCurrentTab(UrlLoadParams* params);
  virtual void LoadUrlInNewTab(UrlLoadParams* params);

  __weak id<URLLoadingServiceDelegate> delegate_;
  AppUrlLoadingService* app_service_;
  Browser* browser_;
  UrlLoadingNotifier* notifier_;
  id<UrlLoader> url_loader_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
