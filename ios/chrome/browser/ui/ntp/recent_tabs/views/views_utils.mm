// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/views/views_utils.h"

#include "base/logging.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Text color.
const int kTextColorBlue = 0x4285f4;
const int kTextColorGray = 0x333333;

// Subtitle text color.
const int kSubtitleColorBlue = 0x7daeff;
const int kSubtitleColorGray = 0x969696;

// Colors for the icons.
const int kIconColorBlue = 0x4285f4;
const int kIconColorGray = 0x5a5a5a;

// Desired width and height of favicon.
const CGFloat kfaviconWidthHeight = 24;
// Minimum favicon pixel size to retrieve.
const CGFloat kfaviconMinWidthHeight = 16;

}  //  namespace

namespace recent_tabs {

UIImage* DefaultFaviconImage() {
  return NativeImage(IDR_IOS_OMNIBOX_HTTP);
}

UILabel* CreateMultilineLabel(NSString* text) {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  [label setTranslatesAutoresizingMaskIntoConstraints:NO];
  [label setText:text];
  [label setLineBreakMode:NSLineBreakByWordWrapping];
  [label setNumberOfLines:0];
  [label setFont:[MDCTypography body1Font]];
  [label setTextColor:UIColorFromRGB(kTextColorGray)];
  [label setTextAlignment:NSTextAlignmentNatural];
  [label setBackgroundColor:[UIColor whiteColor]];
  return label;
}

UIColor* GetTextColorBlue() {
  return UIColorFromRGB(kTextColorBlue);
}

UIColor* GetTextColorGray() {
  return UIColorFromRGB(kTextColorGray);
}

UIColor* GetSubtitleColorBlue() {
  return UIColorFromRGB(kSubtitleColorBlue);
}

UIColor* GetSubtitleColorGray() {
  return UIColorFromRGB(kSubtitleColorGray);
}

UIColor* GetIconColorBlue() {
  return UIColorFromRGB(kIconColorBlue);
}

UIColor* GetIconColorGray() {
  return UIColorFromRGB(kIconColorGray);
}

void GetFavicon(GURL const& url,
                ios::ChromeBrowserState* browser_state,
                FaviconGetterCompletionBlock block) {
  DCHECK(browser_state);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      sync_service ? sync_service->GetOpenTabsUIDelegate() : NULL;
  scoped_refptr<base::RefCountedMemory> favicon;
  if (open_tabs &&
      open_tabs->GetSyncedFaviconForPageURL(url.spec(), &favicon)) {
    dispatch_queue_t queue =
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
    dispatch_async(queue, ^{
      NSData* pngData =
          [NSData dataWithBytes:favicon->front() length:favicon->size()];
      UIImage* image = [[UIImage alloc] initWithData:pngData];
      dispatch_async(dispatch_get_main_queue(), ^{
        // |UIImage initWithData:| may return nil.
        if (image) {
          block([FaviconAttributes attributesWithImage:image]);
        } else {
          block([FaviconAttributes attributesWithImage:DefaultFaviconImage()]);
        }
      });
    });
    block([FaviconAttributes attributesWithImage:DefaultFaviconImage()]);
    return;
  }

  // Use the FaviconCache if there is no synced favicon.
  FaviconLoader* loader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browser_state);
  if (loader) {
    FaviconAttributes* attr =
        loader->FaviconForUrl(url, kfaviconMinWidthHeight, kfaviconWidthHeight,
                              /*fallback_to_google_server=*/false, block);
    DCHECK(attr);
    block(attr);
    return;
  }
  // Finally returns a default image.
  block([FaviconAttributes attributesWithImage:DefaultFaviconImage()]);
}

}  // namespace recent_tabs
