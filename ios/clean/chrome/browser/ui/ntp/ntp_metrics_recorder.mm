// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_metrics_recorder.h"

#include "base/logging.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ntp_home_metrics.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/clean/chrome/browser/ui/commands/ntp_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPMetricsRecorder ()

// The PrefService in which relevant NTP preferences are stored.
@property(nonatomic, assign, readonly) PrefService* prefService;

@end

@implementation NTPMetricsRecorder

@synthesize prefService = _prefService;

- (instancetype)initWithPrefService:(PrefService*)prefs {
  self = [super init];
  if (self) {
    _prefService = prefs;
  }
  return self;
}

- (void)recordMetricForInvocation:(NSInvocation*)anInvocation {
  // As of now, this recorder supports recording for only |showNTPHomePanel|.
  // So, DCHECK to ensure that other selectors do not trigger this recorder.
  DCHECK(anInvocation.selector == @selector(showNTPHomePanel));
  bool contentSuggestionsEnabled =
      self.prefService->GetBoolean(prefs::kSearchSuggestEnabled);
  if (contentSuggestionsEnabled) {
    ntp_home::RecordNTPImpression(ntp_home::REMOTE_SUGGESTIONS);
  } else {
    ntp_home::RecordNTPImpression(ntp_home::LOCAL_SUGGESTIONS);
  }
}

@end
