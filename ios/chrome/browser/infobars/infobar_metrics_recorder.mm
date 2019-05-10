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
// Modal.
const char kInfobarConfirmModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeConfirm";

// Histogram names for InfobarTypePasswordSave.
// Banner.
const char kInfobarPasswordSaveBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePasswordSave";
const char kInfobarPasswordSaveBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePasswordSave";
// Modal.
const char kInfobarPasswordSaveModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypePasswordSave";

// Histogram names for InfobarTypePasswordUpdate.
// Banner.
const char kInfobarPasswordUpdateBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePasswordUpdate";
const char kInfobarPasswordUpdateBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePasswordUpdate";
// Modal.
const char kInfobarPasswordUpdateModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypePasswordUpdate";

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
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveBannerEventHistogram,
                                event);
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordUpdateBannerEventHistogram,
                                event);
      break;
  }
}

- (void)recordBannerDismissType:(MobileMessagesBannerDismissType)dismissType {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(
          kInfobarPasswordUpdateBannerDismissTypeHistogram, dismissType);
      break;
  }
}

- (void)recordModalEvent:(MobileMessagesModalEvent)event {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordUpdateModalEventHistogram,
                                event);
      break;
  }
}

@end
