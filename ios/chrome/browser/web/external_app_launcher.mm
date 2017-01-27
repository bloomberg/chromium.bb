// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/external_app_launcher.h"

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/open_url_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

typedef void (^AlertHandler)(UIAlertAction* action);

// Returns a set of NSStrings that are URL schemes for iTunes Stores.
NSSet<NSString*>* ITMSSchemes() {
  static NSSet<NSString*>* schemes;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    schemes =
        [[NSSet setWithObjects:@"itms", @"itmss", @"itms-apps", @"itms-appss",
                               // There's no evidence that itms-bookss is
                               // actually supported, but over-inclusion
                               // costs less than under-inclusion.
                               @"itms-books", @"itms-bookss", nil] retain];
  });
  return schemes;
}

bool UrlHasAppStoreScheme(const GURL& gURL) {
  std::string scheme = gURL.scheme();
  return [ITMSSchemes() containsObject:base::SysUTF8ToNSString(scheme)];
}

// Logs an entry for |Tab.ExternalApplicationOpened|.  If the user decided to
// open in an external app, pass true.  Otherwise, if the user cancelled the
// opening, pass false.
void RecordExternalApplicationOpened(bool opened) {
  UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened", opened);
}

}  // namespace

@interface ExternalAppLauncher ()
// Returns the Phone/FaceTime call argument from |URL|.
+ (NSString*)formatCallArgument:(NSURL*)URL;
// Ask user for confirmation before dialing FaceTime destination.
- (void)openFaceTimePromptForURL:(NSURL*)telURL;
// Ask user for confirmation before moving to external app.
- (void)openExternalAppWithPromptForURL:(NSURL*)URL;
// Presents a configured alert controller on the root view controller.
- (void)presentAlertControllerWithMessage:(NSString*)message
                                openTitle:(NSString*)openTitle
                              openHandler:(AlertHandler)openHandler
                            cancelHandler:(AlertHandler)cancelHandler;
@end

@implementation ExternalAppLauncher

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

- (void)openExternalAppWithPromptForURL:(NSURL*)URL {
  NSString* message = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
  NSString* openTitle =
      l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
  [self presentAlertControllerWithMessage:message
      openTitle:openTitle
      openHandler:^(UIAlertAction* action) {
        RecordExternalApplicationOpened(true);
        OpenUrlWithCompletionHandler(URL, nil);
      }
      cancelHandler:^(UIAlertAction* action) {
        RecordExternalApplicationOpened(false);
      }];
}

- (void)openFaceTimePromptForURL:(NSURL*)URL {
  NSString* openTitle = l10n_util::GetNSString(IDS_IOS_FACETIME_BUTTON);
  [self presentAlertControllerWithMessage:[[self class] formatCallArgument:URL]
                                openTitle:openTitle
                              openHandler:^(UIAlertAction* action) {
                                OpenUrlWithCompletionHandler(URL, nil);
                              }
                            cancelHandler:nil];
}

- (void)presentAlertControllerWithMessage:(NSString*)message
                                openTitle:(NSString*)openTitle
                              openHandler:(AlertHandler)openHandler
                            cancelHandler:(AlertHandler)cancelHandler {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* openAction =
      [UIAlertAction actionWithTitle:openTitle
                               style:UIAlertActionStyleDefault
                             handler:openHandler];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:cancelHandler];
  [alertController addAction:cancelAction];
  [alertController addAction:openAction];

  [[[[UIApplication sharedApplication] keyWindow] rootViewController]
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (BOOL)openURL:(const GURL&)gURL linkClicked:(BOOL)linkClicked {
  if (!gURL.is_valid() || !gURL.has_scheme())
    return NO;
  NSURL* URL = net::NSURLWithGURL(gURL);

  if (gURL.SchemeIs("facetime") || gURL.SchemeIs("facetime-audio")) {
    // Showing an alert view immediately has a side-effect where focus is
    // taken from the UIWebView so quickly that mouseup events are lost and
    // buttons get 'stuck' in the on position. The solution is to defer
    // showing the view.
    [self performSelector:@selector(openFaceTimePromptForURL:)
               withObject:URL
               afterDelay:0.0];
    return YES;
  }

  // Use telprompt instead of tel because telprompt returns user back to
  // Chrome after phone call is completed/aborted.
  if (gURL.SchemeIs("tel")) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("telprompt");
    URL = net::NSURLWithGURL(gURL.ReplaceComponents(replacements));
    DCHECK([[URL scheme] isEqualToString:@"telprompt"]);
  }

  // Don't open external application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive)
    return NO;

  if (experimental_flags::IsExternalApplicationPromptEnabled()) {
    // Prompt user to open itunes when opening it is not a result of a link
    // click.
    if (!linkClicked && UrlHasAppStoreScheme(gURL)) {
      [self performSelector:@selector(openExternalAppWithPromptForURL:)
                 withObject:URL
                 afterDelay:0.0];
      return YES;
    }
  }

  // If the following call returns YES, an external application is about to be
  // launched and Chrome will go into the background now.
  // TODO(crbug.com/622735): This call still needs to be updated.
  // It's heavily nested so some refactoring is needed.
  return [[UIApplication sharedApplication] openURL:URL];
}

@end
