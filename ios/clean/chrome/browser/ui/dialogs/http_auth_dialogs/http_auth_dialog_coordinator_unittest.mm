// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_coordinator.h"

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/overlay_coordinator_test.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A test fixture for HTTPAuthDialogCoordinators.
class HTTPAuthDialogCoordinatorTest : public OverlayCoordinatorTest {
 public:
  HTTPAuthDialogCoordinatorTest() : OverlayCoordinatorTest() {
    NSURLProtectionSpace* protection_space =
        [[NSURLProtectionSpace alloc] initWithHost:@"host"
                                              port:8080
                                          protocol:@"protocol"
                                             realm:@"realm"
                              authenticationMethod:@"authMethod"];
    NSURLCredential* credential = [[NSURLCredential alloc]
        initWithUser:@"user"
            password:@"password"
         persistence:NSURLCredentialPersistencePermanent];
    HTTPAuthDialogRequest* request =
        [HTTPAuthDialogRequest requestWithWebState:&web_state_
                                   protectionSpace:protection_space
                                        credential:credential
                                          callback:^(NSString*, NSString*){
                                              // no-op.
                                          }];
    coordinator_ = [[HTTPAuthDialogCoordinator alloc] initWithRequest:request];
  }

 protected:
  web::TestWebState web_state_;
  __strong HTTPAuthDialogCoordinator* coordinator_;

  OverlayCoordinator* GetOverlay() override { return coordinator_; }
};

// Tests that an HTTPAuthDialogCoordinator can be initialized and started.
TEST_F(HTTPAuthDialogCoordinatorTest, InitAndStart) {
  StartOverlay();
}
