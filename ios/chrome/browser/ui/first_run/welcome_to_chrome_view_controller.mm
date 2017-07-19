// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#include "ios/chrome/browser/ui/file_locations.h"
#import "ios/chrome/browser/ui/first_run/first_run_chrome_signin_view_controller.h"
#include "ios/chrome/browser/ui/first_run/first_run_util.h"
#include "ios/chrome/browser/ui/first_run/static_file_view_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kUMAMetricsButtonAccessibilityIdentifier =
    @"UMAMetricsButtonAccessibilityIdentifier";

namespace {

const CGFloat kFadeOutAnimationDuration = 0.16f;

// Default value for metrics reporting state. "YES" corresponding to "opt-out"
// state.
const BOOL kDefaultStatsCheckboxValue = YES;
}

@interface WelcomeToChromeViewController ()<WelcomeToChromeViewDelegate> {
  ios::ChromeBrowserState* browserState_;  // weak
  __weak TabModel* tabModel_;
}

// The animation which occurs at launch has run.
@property(nonatomic, assign) BOOL ranLaunchAnimation;

@property(nonatomic, readonly, weak) id<ApplicationSettingsCommands> dispatcher;

@end

@implementation WelcomeToChromeViewController

@synthesize ranLaunchAnimation = _ranLaunchAnimation;
@synthesize dispatcher = _dispatcher;

+ (BOOL)defaultStatsCheckboxValue {
  // Record metrics reporting as opt-in/opt-out only once.
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    metrics::RecordMetricsReportingDefaultState(
        GetApplicationContext()->GetLocalState(),
        kDefaultStatsCheckboxValue ? metrics::EnableMetricsDefault::OPT_OUT
                                   : metrics::EnableMetricsDefault::OPT_IN);
  });
  return kDefaultStatsCheckboxValue;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            tabModel:(TabModel*)tabModel
                          dispatcher:
                              (id<ApplicationSettingsCommands>)dispatcher {
  DCHECK(browserState);
  DCHECK(tabModel);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    browserState_ = browserState;
    tabModel_ = tabModel;
    _dispatcher = dispatcher;
  }
  return self;
}

- (instancetype)initWithNibName:(nullable NSString*)nibNameOrNil
                         bundle:(nullable NSBundle*)nibBundleOrNil {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(nonnull NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)loadView {
  WelcomeToChromeView* welcomeToChromeView =
      [[WelcomeToChromeView alloc] initWithFrame:CGRectZero];
  [welcomeToChromeView setDelegate:self];
  [welcomeToChromeView
      setCheckBoxSelected:[[self class] defaultStatsCheckboxValue]];
  self.view = welcomeToChromeView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.navigationController setNavigationBarHidden:YES];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.ranLaunchAnimation)
    return;
  WelcomeToChromeView* view =
      base::mac::ObjCCastStrict<WelcomeToChromeView>(self.view);
  [view runLaunchAnimation];
  self.ranLaunchAnimation = YES;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (NSURL*)newTermsOfServiceUrl {
  std::string tos = GetTermsOfServicePath();
  NSString* path = [[base::mac::FrameworkBundle() bundlePath]
      stringByAppendingPathComponent:base::SysUTF8ToNSString(tos)];
  NSURLComponents* components = [[NSURLComponents alloc] init];
  [components setScheme:@"file"];
  [components setHost:@""];
  [components setPath:path];
  return [components URL];
}

// Displays the file at the given URL in a StaticFileViewController.
- (void)openStaticFileWithURL:(NSURL*)url title:(NSString*)title {
  StaticFileViewController* staticViewController =
      [[StaticFileViewController alloc] initWithBrowserState:browserState_
                                                         URL:url];
  [staticViewController setTitle:title];
  [self.navigationController pushViewController:staticViewController
                                       animated:YES];
}

#pragma mark - WelcomeToChromeViewDelegate

- (void)welcomeToChromeViewDidTapTOSLink:(WelcomeToChromeView*)view {
  NSString* title = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_TERMS_TITLE);
  NSURL* tosUrl = [self newTermsOfServiceUrl];
  [self openStaticFileWithURL:tosUrl title:title];
}

- (void)welcomeToChromeViewDidTapOKButton:(WelcomeToChromeView*)view {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, view.checkBoxSelected);

  FirstRunConfiguration* firstRunConfig = [[FirstRunConfiguration alloc] init];
  bool hasSSOAccounts = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->HasIdentities();
  [firstRunConfig setHasSSOAccount:hasSSOAccounts];
  FirstRunChromeSigninViewController* signInController =
      [[FirstRunChromeSigninViewController alloc]
          initWithBrowserState:browserState_
                      tabModel:tabModel_
                firstRunConfig:firstRunConfig
                signInIdentity:nil
                    dispatcher:self.dispatcher];

  CATransition* transition = [CATransition animation];
  transition.duration = kFadeOutAnimationDuration;
  transition.type = kCATransitionFade;
  [self.navigationController.view.layer addAnimation:transition
                                              forKey:kCATransition];
  [self.navigationController pushViewController:signInController animated:NO];
}

@end
