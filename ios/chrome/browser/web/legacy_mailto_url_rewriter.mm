// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/legacy_mailto_url_rewriter.h"

#import <Foundation/Foundation.h>

#import "base/logging.h"
#import "ios/chrome/browser/web/mailto_handler.h"
#import "ios/chrome/browser/web/mailto_handler_gmail.h"
#import "ios/chrome/browser/web/mailto_handler_system_mail.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyMailtoURLRewriter ()

// Dictionary keyed by the App Store ID of the Mail client and the value is
// the MailtoHandler object that can rewrite a mailto: URL.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, MailtoHandler*>* handlers;

// Custom logic to handle the migration from Google Native App Launcher options
// to this simplified mailto: URL only system. This must be called after
// -addMailtoApp: has been called to add all the known Mail client apps.
// TODO(crbug.com/718601): At some point in the future when almost all users
// have upgraded, this method can be removed.
- (void)migrateLegacyOptions;

// For users who have not made an explicit choice (kMailtoDefaultHandlerKey
// not set), if Gmail is detected, make an explicit choice for the user to
// use Gmail app as the default mailto: handler.
- (void)autoDefaultToGmailIfInstalled;

@end

@implementation LegacyMailtoURLRewriter
@synthesize handlers = _handlers;

+ (NSString*)userDefaultsKey {
  // The key for NSUserDefaults to store the Mail client selected to handle
  // mailto: URL scheme. If this key is not set, user has not made an explicit
  // choice for default mailto: handler and system-provided Mail client app will
  // be used.
  return @"MailtoHandlerDefault";
}

+ (instancetype)mailtoURLRewriterWithStandardHandlers {
  id result = [[LegacyMailtoURLRewriter alloc] init];
  [result setDefaultHandlers:@[
    [[MailtoHandlerSystemMail alloc] init], [[MailtoHandlerGmail alloc] init]
  ]];
  return result;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _handlers = [NSMutableDictionary dictionary];
  }
  return self;
}

- (NSArray<MailtoHandler*>*)defaultHandlers {
  return [[_handlers allValues]
      sortedArrayUsingComparator:^NSComparisonResult(
          MailtoHandler* _Nonnull obj1, MailtoHandler* _Nonnull obj2) {
        return [[obj1 appName] compare:[obj2 appName]];
      }];
}

- (void)setDefaultHandlers:(NSArray<MailtoHandler*>*)handlerApps {
  for (MailtoHandler* app in handlerApps) {
    [_handlers setObject:app forKey:[app appStoreID]];
  }
  [self migrateLegacyOptions];
  [self autoDefaultToGmailIfInstalled];
}

- (NSString*)defaultHandlerID {
  NSString* value = [[NSUserDefaults standardUserDefaults]
      stringForKey:[[self class] userDefaultsKey]];
  // This implementation of MailtoURLRewriter always returns a non-nil mailto:
  // URL handler ID.
  if ([_handlers[value] isAvailable])
    return value;
  return [[self class] systemMailApp];
}

- (void)setDefaultHandlerID:(NSString*)appStoreID {
  // This implementation of MailtoURLRewriter does not allow the unsetting of
  // a mailto: URL handler.
  DCHECK([appStoreID length]);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* defaultsKey = [[self class] userDefaultsKey];
  if ([appStoreID isEqual:[defaults objectForKey:defaultsKey]])
    return;
  [defaults setObject:appStoreID forKey:defaultsKey];
  [self.observer rewriterDidChange:self];
}

- (NSString*)defaultHandlerName {
  NSString* handlerID = [self defaultHandlerID];
  MailtoHandler* handler = _handlers[handlerID];
  return [handler appName];
}

- (MailtoHandler*)defaultHandlerByID:(NSString*)handlerID {
  return _handlers[handlerID];
}

- (NSString*)rewriteMailtoURL:(const GURL&)URL {
  NSString* value = [self defaultHandlerID];
  if ([value length]) {
    MailtoHandler* handler = _handlers[value];
    if ([handler isAvailable]) {
      return [handler rewriteMailtoURL:URL];
    }
  }
  return nil;
}

#pragma mark - Private

//
// Implements the migration logic for users of previous versions of Google
// Chrome which supports Google Native App Launcher. The goal is to preserve
// the previous behavior and support user choice of non-system provided Mail
// client apps. System-provided Mail client app will be referred to as
// "System Mail". The migration goals are:
// - If a user has not made a choice for preferred mailto: handler in the past,
//   the behavior after upgrading to this version should not change.
// - If a user had disabled Gmail app as the preferred mailto: handler in the
//   past, System Mail should be selected as the default mailto: handler.
// - If a user installs Gmail app after the installation of Chrome, Gmail app
//   will be chosen as the default mailto: handler, preserving the previous
//   behavior.
// - If a user installs another 3rd party mail client app, assuming that the
//   3rd party mail client app is explicitly supported in Chrome, the new 3rd
//   party mail client app can be set as the default mailto: handler through
//   Tools > Settings > Content Settings.
//
// Two NSUserDefaults keys are interpreted with the following meanings:
// - kLegacyShouldAutoOpenKey: The existence of this key implies that the user
//   has used Chrome in the past and had Gmail app installed. Gmail app may or
//   may not be installed currently.
// - kMailtoDefaultHandlerKey: If this key is not set, System Mail app is used
//   to handle mailto: URLs. If this key is set, the value is a string that is
//   the key to |_handlers| which maps to a MailtoHandler object.
//
- (void)migrateLegacyOptions {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* defaultsKey = [[self class] userDefaultsKey];

  // User previously had a selection made for opening mailto: links with Gmail,
  // upgrade will set Gmail app to be the default mailto: handler. If user had
  // opted out to using Gmail app (even when it was installed), migrate user
  // to use system-provided Mail client.
  // The key used in NSUserDefaults is from legacy code when Native App
  // Launcher was still in use. The general format is a string prefix,
  // underscore, then followed by the App Store ID of the application.
  NSString* const kGmailAppStoreID = @"422689480";
  NSString* const kLegacyShouldAutoOpenKey =
      [NSString stringWithFormat:@"ShouldAutoOpenLinks_%@", kGmailAppStoreID];
  NSNumber* legacyValue = [defaults objectForKey:kLegacyShouldAutoOpenKey];
  if (legacyValue) {
    switch ([legacyValue intValue]) {
      case 0:
      case 2: {
        // If legacy user was using default (kAutoOpenLinksNotSet) or had
        // explicitly chosen to use Gmail (kAutoOpenLinksYes), migrate to use
        // Gmail app.
        MailtoHandler* gmailHandler = _handlers[kGmailAppStoreID];
        if ([gmailHandler isAvailable])
          [defaults setObject:kGmailAppStoreID forKey:defaultsKey];
        else
          [defaults removeObjectForKey:defaultsKey];
        break;
      }
      case 1:
        // If legacy user was not using Gmail to handle mailto: links
        // (kAutoOpenLinksNo), consider this an explicit user choice and
        // migrate to use system-provided Mail app.
        [defaults setObject:[[self class] systemMailApp] forKey:defaultsKey];
        break;
      default:
        NOTREACHED();
        break;
    }
    [defaults removeObjectForKey:kLegacyShouldAutoOpenKey];
  }
}

- (void)autoDefaultToGmailIfInstalled {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* defaultsKey = [[self class] userDefaultsKey];
  // If a default handler for mailto: has already been set, user had made an
  // explicit choice and no further changes should be done.
  if ([defaults objectForKey:defaultsKey])
    return;

  NSString* const kGmailAppStoreID = @"422689480";
  MailtoHandler* gmailHandler = _handlers[kGmailAppStoreID];
  if ([gmailHandler isAvailable])
    [defaults setObject:kGmailAppStoreID forKey:defaultsKey];
}

@end
