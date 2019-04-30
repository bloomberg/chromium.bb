// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_manager_impl.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for OverlayManagerImpl.
class OverlayManagerImplTest : public PlatformTest {
 public:
  OverlayManagerImplTest() : web_state_list_(&web_state_list_delegate_) {
    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder browser_state_builder;
    chrome_browser_state_ = browser_state_builder.Build();
    // Create the Browser.
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             &web_state_list_);
    // Use the kWebContentArea modality manager for testing.
    OverlayManagerImpl::Container::CreateForUserData(browser_.get(),
                                                     browser_.get());
    manager_ = OverlayManagerImpl::Container::FromUserData(browser_.get())
                   ->ManagerForModality(OverlayModality::kWebContentArea);
  }
  OverlayManagerImpl* manager() const { return manager_; }
  web::WebState* web_state() const { return web_state_list_.GetWebStateAt(0); }

 private:
  web::TestWebThreadBundle thread_bundle_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  OverlayManagerImpl* manager_;
};
