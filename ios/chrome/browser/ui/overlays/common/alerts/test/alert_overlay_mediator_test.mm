// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/common/alerts/test/alert_overlay_mediator_test.h"

#import "ios/chrome/browser/ui/alert_view/test/fake_alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AlertOverlayMediatorTest::SetMediator(AlertOverlayMediator* mediator) {
  mediator_ = mediator;
  consumer_ = [[FakeAlertConsumer alloc] init];
  mediator_.consumer = consumer_;
}
