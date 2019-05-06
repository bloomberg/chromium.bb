// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Histogram names for InfobarTypeConfirm.
// Banner.
const char kInfobarConfirmBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeConfirm";
const char kInfobarConfirmBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeConfirm";

// Histogram names for InfobarTypePassword.
// Banner.
const char kInfobarPasswordBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePassword";
const char kInfobarPasswordBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePassword";

}  // namespace

@interface InfobarMetricsRecorder ()
// The Infobar type for the metrics recorder.
@property(nonatomic, assign) InfobarType infobarType;
@end

@implementation InfobarMetricsRecorder

#pragma mark - Public

- (instancetype)initWithType:(InfobarType)infobarType {
  self = [super init];
  if (self) {
    _infobarType = infobarType;
  }
  return self;
}

- (void)recordBannerEvent:(MobileMessagesBannerEvent)event {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmBannerEventHistogram, event);
      break;
    case InfobarType::kInfobarTypePassword:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordBannerEventHistogram, event);
      break;
  }
}

- (void)recordBannerDismissType:(MobileMessagesBannerDismissType)dismissType {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypePassword:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordBannerDismissTypeHistogram,
                                dismissType);
      break;
  }
}

@end
