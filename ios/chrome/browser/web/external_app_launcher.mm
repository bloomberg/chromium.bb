// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/external_app_launcher.h"

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/open_url_util.h"
#import "ios/chrome/browser/ui/external_app/open_mail_handler_view_controller.h"
#import "ios/chrome/browser/web/external_apps_launch_policy_decider.h"
#import "ios/chrome/browser/web/mailto_handler.h"
#import "ios/chrome/browser/web/mailto_url_rewriter.h"
#import "ios/chrome/browser/web/nullable_mailto_url_rewriter.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/third_party/material_components_ios/src/components/BottomSheet/src/MDCBottomSheetController.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a set of NSStrings that are URL schemes for iTunes Stores.
NSSet<NSString*>* ITMSSchemes() {
  static NSSet<NSString*>* schemes;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    schemes = [NSSet<NSString*>
        setWithObjects:@"itms", @"itmss", @"itms-apps", @"itms-appss",
                       // There's no evidence that itms-bookss is actually
                       // supported, but over-inclusion costs less than
                       // under-inclusion.
                       @"itms-books", @"itms-bookss", nil];
  });
  return schemes;
}

bool UrlHasAppStoreScheme(const GURL& gURL) {
  std::string scheme = gURL.scheme();
  return [ITMSSchemes() containsObject:base::SysUTF8ToNSString(scheme)];
}

// Returns whether gURL has the scheme of a URL that initiates a call.
bool UrlHasPhoneCallScheme(const GURL& gURL) {
  return gURL.SchemeIs("tel") || gURL.SchemeIs("facetime") ||
         gURL.SchemeIs("facetime-audio");
}

// Returns a string to be used as the label for the prompt's action button.
NSString* PromptActionString(NSString* scheme) {
  if ([scheme isEqualToString:@"facetime"])
    return l10n_util::GetNSString(IDS_IOS_FACETIME_BUTTON);
  else if ([scheme isEqualToString:@"tel"] ||
           [scheme isEqualToString:@"facetime-audio"])
    return l10n_util::GetNSString(IDS_IOS_PHONE_CALL_BUTTON);
  NOTREACHED();
  return @"";
}

// Launches the mail client app represented by |handler| and records metrics.
void LaunchMailClientApp(const GURL& URL, MailtoHandler* handler) {
  NSString* launchURL = [handler rewriteMailtoURL:URL];
  UMA_HISTOGRAM_BOOLEAN("IOS.MailtoURLRewritten", launchURL != nil);
  NSURL* URLToOpen = [launchURL length] ? [NSURL URLWithString:launchURL]
                                        : net::NSURLWithGURL(URL);
  [[UIApplication sharedApplication] openURL:URLToOpen];
}

}  // namespace

@interface ExternalAppLauncher ()
// Returns the Phone/FaceTime call argument from |URL|.
+ (NSString*)formatCallArgument:(NSURL*)URL;

// Returns whether there is a prompt shown by |requestToOpenURL| or not.
@property(nonatomic, getter=isPromptActive) BOOL promptActive;
// Used to check for repeated launches and provide policy for launching apps.
@property(nonatomic, strong) ExternalAppsLaunchPolicyDecider* policyDecider;

// Shows a prompt in Material Design for the user to choose which mail client
// app to use to handle a mailto:// URL.
- (void)promptForMailClientWithURL:(const GURL&)URL
                       URLRewriter:(MailtoURLRewriter*)rewriter;
// Presents an alert controller with |prompt| and |openLabel| as button label
// on the root view controller before launching an external app identified by
// |URL|.
- (void)openExternalAppWithURL:(NSURL*)URL
                        prompt:(NSString*)prompt
                     openLabel:(NSString*)openLabel;

// Opens URL in an external application if possible (optionally after
// confirming via dialog in case that user didn't interact using
// |linkClicked| or if the external application is face time) or returns NO
// if there is no such application available.
- (BOOL)openURL:(const GURL&)gURL linkClicked:(BOOL)linkClicked;
// Presents an alert controller on the root view controller with |prompt| as
// body text, |accept label| and |reject label| as button labels, and
// a non null |responseHandler| that takes a boolean to handle user response.
- (void)showExternalAppLauncherPrompt:(NSString*)prompt
                          acceptLabel:(NSString*)acceptLabel
                          rejectLabel:(NSString*)rejectLabel
                      responseHandler:(void (^_Nonnull)(BOOL))responseHandler;
@end

@implementation ExternalAppLauncher

@synthesize promptActive = _promptActive;
@synthesize policyDecider = _policyDecider;

+ (NSString*)formatCallArgument:(NSURL*)URL {
  NSCharacterSet* charSet =
      [NSCharacterSet characterSetWithCharactersInString:@"/"];
  NSString* scheme = [NSString stringWithFormat:@"%@:", [URL scheme]];
  NSString* URLString = [URL absoluteString];
  if ([URLString length] <= [scheme length])
    return URLString;
  NSString* prompt = [[[[URL absoluteString] substringFromIndex:[scheme length]]
      stringByTrimmingCharactersInSet:charSet] stringByRemovingPercentEncoding];
  // Returns original URL string if there's nothing interesting to display
  // other than the scheme itself.
  if (![prompt length])
    return URLString;
  return prompt;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _policyDecider = [[ExternalAppsLaunchPolicyDecider alloc] init];
  }
  return self;
}

- (void)promptForMailClientWithURL:(const GURL&)URL
                       URLRewriter:(MailtoURLRewriter*)rewriter {
  GURL copiedURLToOpen = URL;
  OpenMailHandlerViewController* mailHandlerChooser =
      [[OpenMailHandlerViewController alloc]
          initWithRewriter:rewriter
           selectedHandler:^(MailtoHandler* _Nonnull handler) {
             LaunchMailClientApp(copiedURLToOpen, handler);
           }];
  MDCBottomSheetController* bottomSheet = [[MDCBottomSheetController alloc]
      initWithContentViewController:mailHandlerChooser];
  [[[[UIApplication sharedApplication] keyWindow] rootViewController]
      presentViewController:bottomSheet
                   animated:YES
                 completion:nil];
}

- (void)showExternalAppLauncherPrompt:(NSString*)prompt
                          acceptLabel:(NSString*)acceptLabel
                          rejectLabel:(NSString*)rejectLabel
                      responseHandler:(void (^_Nonnull)(BOOL))responseHandler {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:nil
                                          message:prompt
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* acceptAction =
      [UIAlertAction actionWithTitle:acceptLabel
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               responseHandler(YES);
                             }];
  UIAlertAction* rejectAction =
      [UIAlertAction actionWithTitle:rejectLabel
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               responseHandler(NO);
                             }];
  [alertController addAction:rejectAction];
  [alertController addAction:acceptAction];

  [[[[UIApplication sharedApplication] keyWindow] rootViewController]
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)openExternalAppWithURL:(NSURL*)URL
                        prompt:(NSString*)prompt
                     openLabel:(NSString*)openLabel {
  [self showExternalAppLauncherPrompt:prompt
                          acceptLabel:openLabel
                          rejectLabel:l10n_util::GetNSString(IDS_CANCEL)
                      responseHandler:^(BOOL accept) {
                        if (accept)
                          OpenUrlWithCompletionHandler(URL, nil);
                        UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened",
                                              accept);
                      }];
}

- (BOOL)requestToOpenURL:(const GURL&)gURL
           sourcePageURL:(const GURL&)sourcePageURL
             linkClicked:(BOOL)linkClicked {
  if (!gURL.is_valid() || !gURL.has_scheme())
    return NO;

  // Don't open external application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return NO;
  }

  // Don't try to open external application if a prompt is already active.
  if (_promptActive)
    return NO;

  [_policyDecider didRequestLaunchExternalAppURL:gURL
                               fromSourcePageURL:sourcePageURL];
  ExternalAppLaunchPolicy policy =
      [_policyDecider launchPolicyForURL:gURL fromSourcePageURL:sourcePageURL];
  switch (policy) {
    case ExternalAppLaunchPolicyBlock: {
      return NO;
    }
    case ExternalAppLaunchPolicyAllow: {
      return [self openURL:gURL linkClicked:linkClicked];
    }
    case ExternalAppLaunchPolicyPrompt: {
      __weak ExternalAppLauncher* weakSelf = self;
      GURL appURL = gURL;
      GURL sourceURL = sourcePageURL;
      _promptActive = YES;
      NSString* promptBody =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP);
      NSString* allowLabel =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_ALLOW);
      NSString* blockLabel =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_BLOCK);

      [self
          showExternalAppLauncherPrompt:promptBody
                            acceptLabel:allowLabel
                            rejectLabel:blockLabel
                        responseHandler:^(BOOL allowed) {
                          ExternalAppLauncher* strongSelf = weakSelf;
                          if (!strongSelf)
                            return;
                          if (allowed) {
                            // By confirming that user want to launch the
                            // application, there is no need to check for
                            // |linkClicked|.
                            [strongSelf openURL:appURL linkClicked:YES];
                          } else {
                            // TODO(crbug.com/674649): Once non modal
                            // dialogs are implemented, update this to
                            // always prompt instead of blocking the app.
                            [strongSelf.policyDecider
                                blockLaunchingAppURL:appURL
                                   fromSourcePageURL:sourceURL];
                          }
                          UMA_HISTOGRAM_BOOLEAN(
                              "IOS.RepeatedExternalAppPromptResponse", allowed);
                          strongSelf.promptActive = NO;
                        }];
      return YES;
    }
  }
}

- (BOOL)openURL:(const GURL&)gURL linkClicked:(BOOL)linkClicked {
  // Don't open external application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return NO;
  }

  NSURL* URL = net::NSURLWithGURL(gURL);
  if (base::ios::IsRunningOnOrLater(10, 3, 0)) {
    if (UrlHasAppStoreScheme(gURL)) {
      NSString* prompt = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      NSString* openLabel =
          l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
      [self openExternalAppWithURL:URL prompt:prompt openLabel:openLabel];
      return YES;
    }
  } else {
    // Prior to iOS 10.3, iOS does not prompt user when facetime: and
    // facetime-audio: URL schemes are opened, so Chrome needs to present an
    // alert before placing a phone call.
    if (UrlHasPhoneCallScheme(gURL)) {
      [self openExternalAppWithURL:URL
                            prompt:[[self class] formatCallArgument:URL]
                         openLabel:PromptActionString([URL scheme])];
      return YES;
    }
    // Prior to iOS 10.3, Chrome prompts user with an alert before opening
    // App Store when user did not tap on any links and an iTunes app URL is
    // opened. This maintains parity with Safari in pre-10.3 environment.
    if (!linkClicked && UrlHasAppStoreScheme(gURL)) {
      NSString* prompt = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      NSString* openLabel =
          l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
      [self openExternalAppWithURL:URL prompt:prompt openLabel:openLabel];
      return YES;
    }
  }

  // Replaces |URL| with a rewritten URL if it is of mailto: scheme.
  if (gURL.SchemeIs(url::kMailToScheme)) {
    MailtoURLRewriter* rewriter =
        [NullableMailtoURLRewriter mailtoURLRewriterWithStandardHandlers];
    NSString* handlerID = [rewriter defaultHandlerID];
    if (!handlerID) {
      [self promptForMailClientWithURL:gURL URLRewriter:rewriter];
      return YES;
    }
    MailtoHandler* handler = [rewriter defaultHandlerByID:handlerID];
    LaunchMailClientApp(gURL, handler);
    return YES;
  }

  // If the following call returns YES, an external application is about to be
  // launched and Chrome will go into the background now.
  // TODO(crbug.com/622735): This call still needs to be updated.
  // It's heavily nested so some refactoring is needed.
  return [[UIApplication sharedApplication] openURL:URL];
}

@end
