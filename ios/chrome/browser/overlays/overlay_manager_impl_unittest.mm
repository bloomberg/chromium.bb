// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_manager_impl.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The full Browser object is not used by OverlayManagerImpl; just its user data
// support.  base::SupportsUserData has a protected destructor, so a subclass
// must be used.
class TestBrowser : public base::SupportsUserData {
 public:
  TestBrowser() = default;
  ~TestBrowser() override = default;
};
}  // namespace

// Test fixture for OverlayManagerImpl.
class OverlayManagerImplTest : public PlatformTest {
 public:
  OverlayManagerImplTest() : web_state_list_(&web_state_list_delegate_) {
    web_state_list_.InsertWebState(/* index */ 0,
                                   std::make_unique<web::TestWebState>(), 0,
                                   WebStateOpener());
    OverlayManagerImpl::Container::CreateForUserData(&browser_,
                                                     &web_state_list_);
    manager_ = OverlayManagerImpl::Container::FromUserData(&browser_)
                   ->ManagerForModality(OverlayModality::kWebContentArea);
  }
  OverlayManagerImpl* manager() const { return manager_; }
  web::WebState* web_state() const { return web_state_list_.GetWebStateAt(0); }

 private:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  TestBrowser browser_;
  OverlayManagerImpl* manager_;
};
