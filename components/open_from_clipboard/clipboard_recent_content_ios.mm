// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_ios.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "url/gurl.h"
#include "url/url_constants.h"

ClipboardRecentContent* ClipboardRecentContent::GetInstance() {
  return ClipboardRecentContentIOS::GetInstance();
}

// Bridge that forwards pasteboard change notifications to its delegate.
@interface PasteboardNotificationListenerBridge : NSObject {
  ClipboardRecentContentIOS* _delegate;
}
@end

@implementation PasteboardNotificationListenerBridge

- (id)initWithDelegate:(ClipboardRecentContentIOS*)delegate {
  DCHECK(delegate);
  self = [super init];
  if (self) {
    _delegate = delegate;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(pasteboardChangedNotification:)
               name:UIPasteboardChangedNotification
             object:[UIPasteboard generalPasteboard]];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(pasteboardChangedNotification:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIPasteboardChangedNotification
              object:[UIPasteboard generalPasteboard]];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:[UIPasteboard generalPasteboard]];
  [super dealloc];
}

- (void)pasteboardChangedNotification:(NSNotification*)notification {
  _delegate->PasteboardChanged();
}

@end

namespace {
// Key used to store the pasteboard's current change count. If when resuming
// chrome the pasteboard's change count is different from the stored one, then
// it means that the pasteboard's content has changed.
NSString* kPasteboardChangeCountKey = @"PasteboardChangeCount";
// Key used to store the last date at which it was detected that the pasteboard
// changed. It is used to evaluate the age of the pasteboard's content.
NSString* kPasteboardChangeDateKey = @"PasteboardChangeDate";
NSTimeInterval kMaximumAgeOfClipboardInSeconds = 6 * 60 * 60;
// Schemes accepted by the ClipboardRecentContentIOS.
const char* kAuthorizedSchemes[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kDataScheme,
    url::kAboutScheme,
};
}  // namespace

ClipboardRecentContentIOS* ClipboardRecentContentIOS::GetInstance() {
  return Singleton<ClipboardRecentContentIOS>::get();
}

bool ClipboardRecentContentIOS::GetRecentURLFromClipboard(GURL* url) const {
  DCHECK(url);
  if (-[lastPasteboardChangeDate_ timeIntervalSinceNow] >
      kMaximumAgeOfClipboardInSeconds) {
    return false;
  }
  if (urlFromPasteboardCache_.is_valid()) {
    *url = urlFromPasteboardCache_;
    return true;
  }
  return false;
}

void ClipboardRecentContentIOS::PasteboardChanged() {
  urlFromPasteboardCache_ = URLFromPasteboard();
  if (!urlFromPasteboardCache_.is_empty()) {
    base::RecordAction(
        base::UserMetricsAction("MobileOmniboxClipboardChanged"));
  }
  lastPasteboardChangeDate_.reset([[NSDate date] retain]);
  lastPasteboardChangeCount_ = [UIPasteboard generalPasteboard].changeCount;
  SaveToUserDefaults();
}

ClipboardRecentContentIOS::ClipboardRecentContentIOS()
    : ClipboardRecentContent() {
  urlFromPasteboardCache_ = URLFromPasteboard();
  LoadFromUserDefaults();
  // The pasteboard's changeCount is reset to zero when the device is restarted.
  // This means that even if |changeCount| hasn't changed, the pasteboard
  // content could have changed. In order to avoid missing pasteboard changes,
  // the changeCount is reset if the device has restarted.
  NSInteger changeCount = [UIPasteboard generalPasteboard].changeCount;
  if (changeCount != lastPasteboardChangeCount_ ||
      DeviceRestartedSincePasteboardChanged()) {
    PasteboardChanged();
  }
  notificationBridge_.reset(
      [[PasteboardNotificationListenerBridge alloc] initWithDelegate:this]);
}

ClipboardRecentContentIOS::~ClipboardRecentContentIOS() {
}

GURL ClipboardRecentContentIOS::URLFromPasteboard() {
  const std::string clipboard =
      base::SysNSStringToUTF8([[UIPasteboard generalPasteboard] string]);
  GURL gurl = GURL(clipboard);
  if (gurl.is_valid()) {
    for (size_t i = 0; i < arraysize(kAuthorizedSchemes); ++i) {
      if (gurl.SchemeIs(kAuthorizedSchemes[i])) {
        return gurl;
      }
    }
    if (!application_scheme_.empty() &&
        gurl.SchemeIs(application_scheme_.c_str())) {
      return gurl;
    }
  }
  return GURL::EmptyGURL();
}

void ClipboardRecentContentIOS::LoadFromUserDefaults() {
  lastPasteboardChangeCount_ = [[NSUserDefaults standardUserDefaults]
      integerForKey:kPasteboardChangeCountKey];
  lastPasteboardChangeDate_.reset([[[NSUserDefaults standardUserDefaults]
      objectForKey:kPasteboardChangeDateKey] retain]);
  DCHECK(!lastPasteboardChangeDate_ ||
         [lastPasteboardChangeDate_ isKindOfClass:[NSDate class]]);
}

void ClipboardRecentContentIOS::SaveToUserDefaults() {
  [[NSUserDefaults standardUserDefaults] setInteger:lastPasteboardChangeCount_
                                             forKey:kPasteboardChangeCountKey];
  [[NSUserDefaults standardUserDefaults] setObject:lastPasteboardChangeDate_
                                            forKey:kPasteboardChangeDateKey];
}

bool ClipboardRecentContentIOS::DeviceRestartedSincePasteboardChanged() {
  int64 secondsSincePasteboardChange =
      -static_cast<int64>([lastPasteboardChangeDate_ timeIntervalSinceNow]);
  int64 secondsSinceLastDeviceRestart = base::SysInfo::Uptime() / 1000;
  return secondsSincePasteboardChange > secondsSinceLastDeviceRestart;
}
