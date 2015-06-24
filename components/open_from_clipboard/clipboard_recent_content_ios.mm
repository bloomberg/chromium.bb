// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_ios.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
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
// Key used to store the
NSString* kSuppressedPasteboardEntryCountKey = @"PasteboardSupressedEntryCount";
base::TimeDelta kMaximumAgeOfClipboard = base::TimeDelta::FromHours(3);
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
  if (GetClipboardContentAge() > kMaximumAgeOfClipboard ||
      [UIPasteboard generalPasteboard].changeCount ==
          suppressedPasteboardEntryCount_) {
    return false;
  }

  if (urlFromPasteboardCache_.is_valid()) {
    *url = urlFromPasteboardCache_;
    return true;
  }
  return false;
}

base::TimeDelta ClipboardRecentContentIOS::GetClipboardContentAge() const {
  return base::TimeDelta::FromSeconds(
      static_cast<int64>(-[lastPasteboardChangeDate_ timeIntervalSinceNow]));
}

void ClipboardRecentContentIOS::SuppressClipboardContent() {
  suppressedPasteboardEntryCount_ =
      [UIPasteboard generalPasteboard].changeCount;
  SaveToUserDefaults();
}

void ClipboardRecentContentIOS::PasteboardChanged() {
  urlFromPasteboardCache_ = URLFromPasteboard();
  if (!urlFromPasteboardCache_.is_empty()) {
    base::RecordAction(
        base::UserMetricsAction("MobileOmniboxClipboardChanged"));
  }
  lastPasteboardChangeDate_.reset([[NSDate date] retain]);
  lastPasteboardChangeCount_ = [UIPasteboard generalPasteboard].changeCount;
  if (lastPasteboardChangeCount_ != suppressedPasteboardEntryCount_) {
    suppressedPasteboardEntryCount_ = NSIntegerMax;
  }
}

ClipboardRecentContentIOS::ClipboardRecentContentIOS() {
  Init(base::TimeDelta::FromMilliseconds(base::SysInfo::Uptime()));
}

ClipboardRecentContentIOS::ClipboardRecentContentIOS(base::TimeDelta uptime) {
  Init(uptime);
}

void ClipboardRecentContentIOS::Init(base::TimeDelta uptime) {
  lastPasteboardChangeCount_ = NSIntegerMax;
  suppressedPasteboardEntryCount_ = NSIntegerMax;
  urlFromPasteboardCache_ = URLFromPasteboard();
  LoadFromUserDefaults();

  // On iOS 7 (unlike on iOS 8, despite what the documentation says), the change
  // count is reset when the device is rebooted.
  if (uptime < GetClipboardContentAge() &&
      !base::ios::IsRunningOnIOS8OrLater()) {
    if ([UIPasteboard generalPasteboard].changeCount == 0) {
      // The user hasn't pasted anything in the clipboard since the device's
      // reboot. |PasteboardChanged| isn't called because it would update
      // |lastPasteboardChangeData_|, and record metrics.
      lastPasteboardChangeCount_ = 0;
      if (suppressedPasteboardEntryCount_ != NSIntegerMax) {
        // If the last time Chrome was running the pasteboard was suppressed,
        // and the user  has not copied anything since the device launched, then
        // supress this entry.
        suppressedPasteboardEntryCount_ = 0;
      }
      SaveToUserDefaults();
    } else {
      // The user pasted something in the clipboard since the device's reboot.
      PasteboardChanged();
    }
  } else {
    NSInteger changeCount = [UIPasteboard generalPasteboard].changeCount;
    if (changeCount != lastPasteboardChangeCount_) {
      PasteboardChanged();
    }
  }
  // Makes sure |lastPasteboardChangeCount_| was properly initialized.
  DCHECK_NE(lastPasteboardChangeCount_, NSIntegerMax);
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
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  lastPasteboardChangeCount_ =
      [defaults integerForKey:kPasteboardChangeCountKey];
  lastPasteboardChangeDate_.reset(
      [[defaults objectForKey:kPasteboardChangeDateKey] retain]);

  if ([[[defaults dictionaryRepresentation] allKeys]
          containsObject:kSuppressedPasteboardEntryCountKey]) {
    suppressedPasteboardEntryCount_ =
        [defaults integerForKey:kSuppressedPasteboardEntryCountKey];
  } else {
    suppressedPasteboardEntryCount_ = NSIntegerMax;
  }

  DCHECK(!lastPasteboardChangeDate_ ||
         [lastPasteboardChangeDate_ isKindOfClass:[NSDate class]]);
}

void ClipboardRecentContentIOS::SaveToUserDefaults() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:lastPasteboardChangeCount_
                forKey:kPasteboardChangeCountKey];
  [defaults setObject:lastPasteboardChangeDate_
               forKey:kPasteboardChangeDateKey];
  [defaults setInteger:suppressedPasteboardEntryCount_
                forKey:kSuppressedPasteboardEntryCountKey];
}
