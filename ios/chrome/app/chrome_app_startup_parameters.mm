// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/chrome_app_startup_parameters.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/xcallback_parameters.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/x_callback_url.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

namespace {

// Key of the UMA Startup.MobileSessionStartAction histogram.
const char kUMAMobileSessionStartActionHistogram[] =
    "Startup.MobileSessionStartAction";

const char kApplicationGroupCommandDelay[] =
    "Startup.ApplicationGroupCommandDelay";

// URL Query String parameter to indicate that this openURL: request arrived
// here due to a Smart App Banner presentation on a Google.com page.
NSString* const kSmartAppBannerKey = @"safarisab";

const CGFloat kAppGroupTriggersVoiceSearchTimeout = 15.0;

// Values of the UMA Startup.MobileSessionStartAction histogram.
enum MobileSessionStartAction {
  START_ACTION_OPEN_HTTP = 0,
  START_ACTION_OPEN_HTTPS,
  START_ACTION_OPEN_FILE,
  START_ACTION_XCALLBACK_OPEN,
  START_ACTION_XCALLBACK_OTHER,
  START_ACTION_OTHER,
  START_ACTION_XCALLBACK_APPGROUP_COMMAND,
  MOBILE_SESSION_START_ACTION_COUNT,
};

}  // namespace

@implementation ChromeAppStartupParameters {
  base::scoped_nsobject<NSString> _secureSourceApp;
  base::scoped_nsobject<NSString> _declaredSourceApp;
  base::scoped_nsobject<NSURL> _completeURL;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                xCallbackParameters:(XCallbackParameters*)xCallbackParameters {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                xCallbackParameters:(XCallbackParameters*)xCallbackParameters
                  declaredSourceApp:(NSString*)declaredSourceApp
                    secureSourceApp:(NSString*)secureSourceApp
                        completeURL:(NSURL*)completeURL {
  self = [super initWithExternalURL:externalURL
                xCallbackParameters:xCallbackParameters];
  if (self) {
    _declaredSourceApp.reset([declaredSourceApp copy]);
    _secureSourceApp.reset([secureSourceApp copy]);
    _completeURL.reset([completeURL retain]);
  }
  return self;
}

+ (instancetype)newChromeAppStartupParametersWithURL:(NSURL*)completeURL
                               fromSourceApplication:(NSString*)appId {
  GURL gurl = net::GURLWithNSURL(completeURL);

  if (!gurl.is_valid() || gurl.scheme().length() == 0)
    return nil;

  // TODO(ios): Temporary fix for b/7174478
  if (IsXCallbackURL(gurl)) {
    NSString* action = [completeURL path];
    // Currently only "open" and "extension-command" are supported.
    // Other actions are being considered (see b/6914153).
    if ([action
            isEqualToString:
                [NSString
                    stringWithFormat:
                        @"/%s", app_group::kChromeAppGroupXCallbackCommand]]) {
      UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                                START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                                MOBILE_SESSION_START_ACTION_COUNT);
      return [ChromeAppStartupParameters
          newExtensionCommandAppStartupParametersFromWithURL:completeURL
                                       fromSourceApplication:appId];
    }

    if (![action isEqualToString:@"/open"]) {
      UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                                START_ACTION_XCALLBACK_OTHER,
                                MOBILE_SESSION_START_ACTION_COUNT);
      return nil;
    }

    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_OPEN,
                              MOBILE_SESSION_START_ACTION_COUNT);

    std::map<std::string, std::string> parameters =
        ExtractQueryParametersFromXCallbackURL(gurl);
    GURL url = GURL(parameters["url"]);
    if (!url.is_valid() ||
        (!url.SchemeIs(url::kHttpScheme) && !url.SchemeIs(url::kHttpsScheme))) {
      return nil;
    }

    base::scoped_nsobject<XCallbackParameters> xcallbackParameters(
        [[XCallbackParameters alloc] initWithSourceAppId:appId]);

    return [[ChromeAppStartupParameters alloc]
        initWithExternalURL:url
        xCallbackParameters:xcallbackParameters
          declaredSourceApp:appId
            secureSourceApp:nil
                completeURL:completeURL];

  } else if (gurl.SchemeIsFile()) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_OPEN_FILE,
                              MOBILE_SESSION_START_ACTION_COUNT);
    // |url| is the path to a file received from another application.
    GURL::Replacements replacements;
    const std::string host(kChromeUIExternalFileHost);
    std::string filename = gurl.ExtractFileName();
    replacements.SetPathStr(filename);
    replacements.SetSchemeStr(kChromeUIScheme);
    replacements.SetHostStr(host);
    GURL externalURL = gurl.ReplaceComponents(replacements);
    if (!externalURL.is_valid())
      return nil;
    return [[ChromeAppStartupParameters alloc] initWithExternalURL:externalURL
                                               xCallbackParameters:nil
                                                 declaredSourceApp:appId
                                                   secureSourceApp:nil
                                                       completeURL:completeURL];
  } else {
    // Replace the scheme with https or http depending on whether the input
    // |url| scheme ends with an 's'.
    BOOL useHttps = gurl.scheme()[gurl.scheme().length() - 1] == 's';
    MobileSessionStartAction action =
        useHttps ? START_ACTION_OPEN_HTTPS : START_ACTION_OPEN_HTTP;
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram, action,
                              MOBILE_SESSION_START_ACTION_COUNT);
    GURL::Replacements replace_scheme;
    if (useHttps)
      replace_scheme.SetSchemeStr(url::kHttpsScheme);
    else
      replace_scheme.SetSchemeStr(url::kHttpScheme);
    GURL externalURL = gurl.ReplaceComponents(replace_scheme);
    if (!externalURL.is_valid())
      return nil;
    return [[ChromeAppStartupParameters alloc] initWithExternalURL:externalURL
                                               xCallbackParameters:nil
                                                 declaredSourceApp:appId
                                                   secureSourceApp:nil
                                                       completeURL:completeURL];
  }
}

+ (instancetype)newExtensionCommandAppStartupParametersFromWithURL:(NSURL*)url
                                             fromSourceApplication:
                                                 (NSString*)appId {
  NSString* appGroup = app_group::ApplicationGroup();
  base::scoped_nsobject<NSUserDefaults> sharedDefaults(
      [[NSUserDefaults alloc] initWithSuiteName:appGroup]);

  NSString* commandDictionaryPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  NSDictionary* commandDictionary = base::mac::ObjCCast<NSDictionary>(
      [sharedDefaults objectForKey:commandDictionaryPreference]);

  [sharedDefaults removeObjectForKey:commandDictionaryPreference];

  // |sharedDefaults| is used for communication between apps. Synchronize to
  // avoid synchronization issues (like removing the next order).
  [sharedDefaults synchronize];

  if (!commandDictionary) {
    return nil;
  }

  NSString* commandCallerPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandCaller = base::mac::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandCallerPreference]);

  NSString* commandPreference = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);
  NSString* command = base::mac::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandPreference]);

  NSString* commandTimePreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  id commandTime = base::mac::ObjCCast<NSDate>(
      [commandDictionary objectForKey:commandTimePreference]);

  NSString* commandParameterPreference = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandParameterPreference);
  NSString* commandParameter = base::mac::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandParameterPreference]);

  if (!commandCaller || !command || !commandTimePreference) {
    return nil;
  }

  // Check the time of the last request to avoid app from intercepting old
  // open url request and replay it later.
  NSTimeInterval delay = [[NSDate date] timeIntervalSinceDate:commandTime];
  UMA_HISTOGRAM_COUNTS_100(kApplicationGroupCommandDelay, delay);
  if (delay > kAppGroupTriggersVoiceSearchTimeout)
    return nil;
  return [ChromeAppStartupParameters
      newAppStartupParametersForCommand:command
                          withParameter:commandParameter
                                withURL:url
                  fromSourceApplication:appId
            fromSecureSourceApplication:commandCaller];
}

+ (instancetype)newAppStartupParametersForCommand:(NSString*)command
                                    withParameter:(id)parameter
                                          withURL:(NSURL*)url
                            fromSourceApplication:(NSString*)appId
                      fromSecureSourceApplication:(NSString*)secureSourceApp {
  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupVoiceSearchCommand)]) {
    ChromeAppStartupParameters* params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
        xCallbackParameters:nil
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    [params setLaunchVoiceSearch:YES];
    return params;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupNewTabCommand)]) {
    return [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
        xCallbackParameters:nil
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupFocusOmniboxCommand)]) {
    ChromeAppStartupParameters* params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
        xCallbackParameters:nil
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    [params setLaunchFocusOmnibox:YES];
    return params;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupOpenURLCommand)]) {
    if (!parameter || ![parameter isKindOfClass:[NSString class]])
      return nil;
    GURL externalURL(base::SysNSStringToUTF8(parameter));
    if (!externalURL.is_valid() || !externalURL.SchemeIsHTTPOrHTTPS())
      return nil;
    return
        [[ChromeAppStartupParameters alloc] initWithExternalURL:externalURL
                                            xCallbackParameters:nil
                                              declaredSourceApp:appId
                                                secureSourceApp:secureSourceApp
                                                    completeURL:url];
  }

  return nil;
}

- (MobileSessionCallerApp)callerApp {
  if ([_secureSourceApp isEqualToString:@"TodayExtension"])
    return CALLER_APP_GOOGLE_CHROME_TODAY_EXTENSION;

  if (![_declaredSourceApp length])
    return CALLER_APP_NOT_AVAILABLE;
  if ([_declaredSourceApp isEqualToString:@"com.google.GoogleMobile"])
    return CALLER_APP_GOOGLE_SEARCH;
  if ([_declaredSourceApp isEqualToString:@"com.google.Gmail"])
    return CALLER_APP_GOOGLE_GMAIL;
  if ([_declaredSourceApp isEqualToString:@"com.google.GooglePlus"])
    return CALLER_APP_GOOGLE_PLUS;
  if ([_declaredSourceApp isEqualToString:@"com.google.Drive"])
    return CALLER_APP_GOOGLE_DRIVE;
  if ([_declaredSourceApp isEqualToString:@"com.google.b612"])
    return CALLER_APP_GOOGLE_EARTH;
  if ([_declaredSourceApp isEqualToString:@"com.google.ios.youtube"])
    return CALLER_APP_GOOGLE_YOUTUBE;
  if ([_declaredSourceApp isEqualToString:@"com.google.Maps"])
    return CALLER_APP_GOOGLE_MAPS;
  if ([_declaredSourceApp hasPrefix:@"com.google."])
    return CALLER_APP_GOOGLE_OTHER;
  if ([_declaredSourceApp isEqualToString:@"com.apple.mobilesafari"])
    return CALLER_APP_APPLE_MOBILESAFARI;
  if ([_declaredSourceApp hasPrefix:@"com.apple."])
    return CALLER_APP_APPLE_OTHER;

  return CALLER_APP_OTHER;
}

- (first_run::ExternalLaunch)launchSource {
  if ([self callerApp] != CALLER_APP_APPLE_MOBILESAFARI) {
    return first_run::LAUNCH_BY_OTHERS;
  }

  NSString* query = [_completeURL query];
  // Takes care of degenerated case of no QUERY_STRING.
  if (![query length])
    return first_run::LAUNCH_BY_MOBILESAFARI;
  // Look for |kSmartAppBannerKey| anywhere within the query string.
  NSRange found = [query rangeOfString:kSmartAppBannerKey];
  if (found.location == NSNotFound)
    return first_run::LAUNCH_BY_MOBILESAFARI;
  // |kSmartAppBannerKey| can be at the beginning or end of the query
  // string and may also be optionally followed by a equal sign and a value.
  // For now, just look for the presence of the key and ignore the value.
  if (found.location + found.length < [query length]) {
    // There are characters following the found location.
    unichar charAfter =
        [query characterAtIndex:(found.location + found.length)];
    if (charAfter != '&' && charAfter != '=')
      return first_run::LAUNCH_BY_MOBILESAFARI;
  }
  if (found.location > 0) {
    unichar charBefore = [query characterAtIndex:(found.location - 1)];
    if (charBefore != '&')
      return first_run::LAUNCH_BY_MOBILESAFARI;
  }
  return first_run::LAUNCH_BY_SMARTAPPBANNER;
}

@end
