// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/app_delegate.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#import "ios/showcase/core/showcase_model.h"
#import "ios/showcase/core/showcase_view_controller.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MDCTypographyAdditions/MDFRobotoFontLoader+MDCTypographyAdditions.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppDelegate
@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  base::MakeUnique<IOSChromeMain>();
  ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ResourceBundle::LOAD_COMMON_RESOURCES);
  [MDCTypography setFontLoader:[MDFRobotoFontLoader sharedInstance]];
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  ShowcaseViewController* viewController =
      [[ShowcaseViewController alloc] initWithRows:[self rowsToDisplay]];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  self.window.rootViewController = navigationController;
  [self.window makeKeyAndVisible];

  return YES;
}

#pragma mark - Private

// Creates model data to display in the view controller.
- (NSArray<showcase::ModelRow*>*)rowsToDisplay {
  NSArray<showcase::ModelRow*>* rows = [ShowcaseModel model];
  NSSortDescriptor* sortDescriptor =
      [NSSortDescriptor sortDescriptorWithKey:showcase::kClassForDisplayKey
                                    ascending:YES];
  return [rows sortedArrayUsingDescriptors:@[ sortDescriptor ]];
}

@end
