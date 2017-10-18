// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns CRWWebController for the given |web_state|.
CRWWebController* GetWebController(web::WebState* web_state) {
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);
  return web_state_impl->GetWebController();
}
}  // namespace

namespace web {

WebTestWithWebState::WebTestWithWebState() {}

WebTestWithWebState::~WebTestWithWebState() {}

void WebTestWithWebState::SetUp() {
  WebTest::SetUp();
  web::WebState::CreateParams params(GetBrowserState());
  web_state_ = web::WebState::Create(params);
  web_state_->SetWebUsageEnabled(true);

  // Force generation of child views; necessary for some tests.
  web_state_->GetView();
}

void WebTestWithWebState::TearDown() {
  DestroyWebState();
  WebTest::TearDown();
}

void WebTestWithWebState::AddPendingItem(const GURL& url,
                                         ui::PageTransition transition) {
  GetWebController(web_state())
      .webStateImpl->GetNavigationManagerImpl()
      .AddPendingItem(url, Referrer(), transition,
                      web::NavigationInitiationType::USER_INITIATED,
                      web::NavigationManager::UserAgentOverrideOption::INHERIT);
}

void WebTestWithWebState::LoadHtml(NSString* html, const GURL& url) {
  // Sets MIME type to "text/html" once navigation is committed.
  class MimeTypeUpdater : public WebStateObserver {
   public:
    explicit MimeTypeUpdater(WebState* web_state)
        : WebStateObserver(web_state) {}
    // WebStateObserver overrides:
    void NavigationItemCommitted(WebState* web_state,
                                 const LoadCommittedDetails&) override {
      // loadHTML:forURL: does not notify web view delegate about received
      // response, so web controller does not get a chance to properly update
      // MIME type and it should be set manually after navigation is committed
      // but before WebState signal load completion and clients will start
      // checking if MIME type is in fact HTML.
      static_cast<WebStateImpl*>(web_state)->SetContentsMimeType("text/html");
    }
  };
  MimeTypeUpdater mime_type_updater(web_state());

  // Initiate asynchronous HTML load.
  CRWWebController* web_controller = GetWebController(web_state());
  ASSERT_EQ(PAGE_LOADED, web_controller.loadPhase);
  [web_controller loadHTML:html forURL:url];
  ASSERT_EQ(LOAD_REQUESTED, web_controller.loadPhase);

  // Wait until the page is loaded.
  base::test::ios::WaitUntilCondition(^{
    return web_controller.loadPhase == PAGE_LOADED;
  });

  // Reload the page if script execution is not possible.
  if (![ExecuteJavaScript(@"0;") isEqual:@0]) {
    LoadHtml(html, url);
  }
}

void WebTestWithWebState::LoadHtml(NSString* html) {
  GURL url("https://chromium.test/");
  LoadHtml(html, url);
}

void WebTestWithWebState::LoadHtml(const std::string& html) {
  LoadHtml(base::SysUTF8ToNSString(html));
}

void WebTestWithWebState::WaitForBackgroundTasks() {
  // Because tasks can add new tasks to either queue, the loop continues until
  // the first pass where no activity is seen from either queue.
  bool activitySeen = false;
  base::MessageLoop* messageLoop = base::MessageLoop::current();
  messageLoop->AddTaskObserver(this);
  do {
    activitySeen = false;

    // Yield to the iOS message queue, e.g. [NSObject performSelector:] events.
    if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) ==
        kCFRunLoopRunHandledSource)
      activitySeen = true;

    // Yield to the Chromium message queue, e.g. WebThread::PostTask()
    // events.
    processed_a_task_ = false;
    base::RunLoop().RunUntilIdle();
    if (processed_a_task_)  // Set in TaskObserver method.
      activitySeen = true;

  } while (activitySeen);
  messageLoop->RemoveTaskObserver(this);
}

void WebTestWithWebState::WaitForCondition(ConditionBlock condition) {
  base::test::ios::WaitUntilCondition(condition, true,
                                      base::TimeDelta::FromSeconds(10));
}

id WebTestWithWebState::ExecuteJavaScript(NSString* script) {
  __block id executionResult;
  __block bool executionCompleted = false;
  [GetWebController(web_state())
      executeJavaScript:script
      completionHandler:^(id result, NSError* error) {
        executionResult = [result copy];
        executionCompleted = true;
      }];
  base::test::ios::WaitUntilCondition(^{
    return executionCompleted;
  });
  return executionResult;
}

void WebTestWithWebState::DestroyWebState() {
  web_state_.reset();
}

std::string WebTestWithWebState::BaseUrl() const {
  web::URLVerificationTrustLevel unused_level;
  return web_state()->GetCurrentURL(&unused_level).spec();
}

web::WebState* WebTestWithWebState::web_state() {
  return web_state_.get();
}

const web::WebState* WebTestWithWebState::web_state() const {
  return web_state_.get();
}

void WebTestWithWebState::WillProcessTask(const base::PendingTask&) {
  // Nothing to do.
}

void WebTestWithWebState::DidProcessTask(const base::PendingTask&) {
  processed_a_task_ = true;
}

}  // namespace web
