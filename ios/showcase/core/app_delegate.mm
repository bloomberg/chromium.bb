// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/app_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#import "ios/showcase/core/showcase_model.h"
#import "ios/showcase/core/showcase_view_controller.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/app/web_main_parts.h"
#import "ios/web/public/web_client.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppDelegate
@synthesize window = _window;

namespace {

class ShowcaseWebMainParts : public web::WebMainParts {
  void PreMainMessageLoopStart() override {
    ResourceBundle::InitSharedInstanceWithLocale(
        std::string(), nullptr, ResourceBundle::LOAD_COMMON_RESOURCES);

    base::FilePath pak_path;
    PathService::Get(base::DIR_MODULE, &pak_path);
    ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("showcase_resources.pak"), ui::SCALE_FACTOR_100P);
  }
};

class ShowcaseWebClient : public web::WebClient {
 public:
  ShowcaseWebClient() {}
  ~ShowcaseWebClient() override {}

  // WebClient implementation.
  std::unique_ptr<web::WebMainParts> CreateWebMainParts() override {
    return base::MakeUnique<ShowcaseWebMainParts>();
  }
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override {
    return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
        resource_id, scale_factor);
  }
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) const override {
    return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
        resource_id);
  }
};
}

- (void)setupUI {
  ShowcaseViewController* viewController =
      [[ShowcaseViewController alloc] initWithRows:[AppDelegate rowsToDisplay]];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  self.window.rootViewController = navigationController;
}

#pragma mark - UIApplicationDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // TODO(crbug.com/738880): Showcase ideally shouldn't be an embedder of
  // //ios/web, in which case it wouldn't have to do this. This almost
  // certainly means not creating IOSChromeMain.
  web::SetWebClient(new ShowcaseWebClient());
  base::MakeUnique<IOSChromeMain>();

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self setupUI];
  [self.window makeKeyAndVisible];

  return YES;
}

#pragma mark - Private

// Creates model data to display in the view controller.
+ (NSArray<showcase::ModelRow*>*)rowsToDisplay {
  NSArray<showcase::ModelRow*>* rows = [ShowcaseModel model];
  NSSortDescriptor* sortDescriptor =
      [NSSortDescriptor sortDescriptorWithKey:showcase::kClassForDisplayKey
                                    ascending:YES];
  return [rows sortedArrayUsingDescriptors:@[ sortDescriptor ]];
}

@end
