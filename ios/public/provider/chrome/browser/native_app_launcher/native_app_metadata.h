// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_METADATA_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_METADATA_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"

@class ChromeIdentity;
class GURL;
@class UIImage;

namespace image_fetcher {
class IOSImageDataFetcherWrapper;
}

namespace net {
class URLRequestContextGetter;
}

// Protocol todo describe
@protocol NativeAppMetadata<NSObject>

// Defines whether the app should be opened automatically upon Link Navigation.
// This is a tri-state value internally: Yes, No, not set. However, externally,
// only the Yes/No state is returned via the property getter. If the internal
// value is "not set", then it is up to the current implementation to decide
// whether it should be treated as a YES or a NO. The property setter can set
// the value to either YES or NO. To set the value to "not set", use
// -unsetShouldAutoOpenLinks.
@property(nonatomic, assign) BOOL shouldAutoOpenLinks;

// Method to set shouldAutoOpenLinks property to "not set".
- (void)unsetShouldAutoOpenLinks;

// Defines whether infobars for this app should be bypassed.
@property(nonatomic, assign) BOOL shouldBypassInfoBars;

// Stores the number of times a banner is dismissed or ignored.
@property(nonatomic, assign) NSInteger numberOfDismissedInfoBars;

// Returns the application name for this native app. If there is a localized
// name for the current locale, return the localized name. Otherwise, return
// the default application name.
- (NSString*)appName;

// Returns the App Store application id for this native app or |nil| if
// app does not have an AppStore ID.
- (NSString*)appId;

// Returns whether this native app is a Google App.
- (BOOL)isGoogleOwnedApp;

// Returns whether this native app is installed.
- (BOOL)isInstalled;

// Returns the URL string that launches Apple AppStore for this app.
- (NSString*)appStoreURL;

// Returns the URL to test if the app is installed.
- (NSURL*)appURLforURL:(NSURL*)url;

// Calls |block| with the application icon. |imageFetcher| must be kept alive
// during the fetch.
- (void)fetchSmallIconWithImageFetcher:
            (image_fetcher::IOSImageDataFetcherWrapper*)imageFetcher
                       completionBlock:(void (^)(UIImage*))block;

// Returns whether this native application can open the |url|.
- (BOOL)canOpenURL:(const GURL&)url;

// Returns the launch URL with which the application can be opened. |gurl| is
// the URL of the content in the web app. If |identity| is not nil, the
// returned URL contains a hash associated with |identity|.
- (GURL)launchURLWithURL:(const GURL&)gurl identity:(ChromeIdentity*)identity;

// Resets values of shouldBypassInfobars and numberOfDismissedInfoBarsKey.
- (void)resetInfobarHistory;

// Enumerates the app's registered schemes. The block can be called multiple
// times for the same scheme.
- (void)enumerateSchemesWithBlock:(void (^)(NSString* scheme, BOOL* stop))block;

// Informs the metadata that user has requested the application to be installed
// from a user interface other than a Launcher infobar.
- (void)updateCounterWithAppInstallation;

// Returns any of the schemes that the app has registered.
- (NSString*)anyScheme;

// This method needs to be called whenever the metadata info is displayed by an
// infobar.
- (void)willBeShownInInfobarOfType:(NativeAppControllerType)type;

// Informs the metadata on what user action on the infobar has been taken.
// Requires to have previously send the message -[NativeAppMetadata
// willBeShownInInfobarOfType:].
- (void)updateWithUserAction:(NativeAppActionType)userAction;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_METADATA_H_
