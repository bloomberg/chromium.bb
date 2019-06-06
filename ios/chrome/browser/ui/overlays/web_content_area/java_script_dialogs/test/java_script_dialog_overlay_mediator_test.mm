// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/test/java_script_dialog_overlay_mediator_test.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_view_controller/test/fake_alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

JavaScriptDialogOverlayMediatorTest::JavaScriptDialogOverlayMediatorTest()
    : PlatformTest(), consumer_([[FakeAlertConsumer alloc] init]) {}

JavaScriptDialogOverlayMediatorTest::~JavaScriptDialogOverlayMediatorTest() =
    default;

void JavaScriptDialogOverlayMediatorTest::SetMediator(
    JavaScriptDialogOverlayMediator* mediator) {
  if (mediator_ == mediator)
    return;
  mediator_.consumer = nil;
  mediator_ = mediator;
  mediator_.consumer = consumer_;
}
