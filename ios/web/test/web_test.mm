// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_test.h"

#include <utility>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/ui/crw_web_delegate.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "third_party/ocmock/OCMock/OCMock.h"

// Helper Mock to stub out API with C++ objects in arguments.
@interface WebDelegateMock : OCMockComplexTypeHelper
@end

@implementation WebDelegateMock
// Stub implementation always returns YES.
- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked {
  return YES;
}
@end

namespace web {

#pragma mark -

WebTest::WebTest() : web_client_(base::WrapUnique(new TestWebClient)) {}
WebTest::~WebTest() {}

void WebTest::SetUp() {
  PlatformTest::SetUp();
  BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
}

void WebTest::TearDown() {
  BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  PlatformTest::TearDown();
}

TestWebClient* WebTest::GetWebClient() {
  return static_cast<TestWebClient*>(web_client_.Get());
}

BrowserState* WebTest::GetBrowserState() {
  return &browser_state_;
}

#pragma mark -

WebTestWithWebController::WebTestWithWebController() {}

WebTestWithWebController::~WebTestWithWebController() {}

static int s_html_load_count;

void WebTestWithWebController::SetUp() {
  WebTest::SetUp();
  web_state_impl_.reset(new WebStateImpl(GetBrowserState()));
  web_state_impl_->GetNavigationManagerImpl().InitializeSession(nil, nil, NO,
                                                                0);
  web_state_impl_->SetWebUsageEnabled(true);
  webController_.reset(web_state_impl_->GetWebController());

  // Force generation of child views; necessary for some tests.
  [webController_ triggerPendingLoad];
  s_html_load_count = 0;
}

void WebTestWithWebController::TearDown() {
  web_state_impl_.reset();
  WebTest::TearDown();
}

void WebTestWithWebController::LoadHtml(NSString* html) {
  LoadHtml([html UTF8String]);
}

void WebTestWithWebController::LoadHtml(const std::string& html) {
  NSString* load_check = CreateLoadCheck();
  std::string marked_html = html + [load_check UTF8String];
  std::string encoded_html;
  base::Base64Encode(marked_html, &encoded_html);
  GURL url("data:text/html;charset=utf8;base64," + encoded_html);
  LoadURL(url);

  if (ResetPageIfNavigationStalled(load_check)) {
    LoadHtml(html);
  }
}

void WebTestWithWebController::LoadURL(const GURL& url) {
  // First step is to ensure that the web controller has finished any previous
  // page loads so the new load is not confused.
  while ([webController_ loadPhase] != PAGE_LOADED)
    WaitForBackgroundTasks();
  id originalMockDelegate = [OCMockObject
      niceMockForProtocol:@protocol(CRWWebDelegate)];
  id mockDelegate = [[WebDelegateMock alloc]
      initWithRepresentedObject:originalMockDelegate];
  id existingDelegate = webController_.get().delegate;
  webController_.get().delegate = mockDelegate;

  web::NavigationManagerImpl& navManager =
      [webController_ webStateImpl]->GetNavigationManagerImpl();
  navManager.InitializeSession(@"name", nil, NO, 0);
  [navManager.GetSessionController()
        addPendingEntry:url
               referrer:web::Referrer()
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];

  [webController_ loadCurrentURL];
  while ([webController_ loadPhase] != PAGE_LOADED)
    WaitForBackgroundTasks();
  webController_.get().delegate = existingDelegate;
  [[webController_ view] layoutIfNeeded];
}

void WebTestWithWebController::WaitForBackgroundTasks() {
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
    messageLoop->RunUntilIdle();
    if (processed_a_task_)  // Set in TaskObserver method.
      activitySeen = true;

  } while (activitySeen);
  messageLoop->RemoveTaskObserver(this);
}

void WebTestWithWebController::WaitForCondition(ConditionBlock condition) {
  base::MessageLoop* messageLoop = base::MessageLoop::current();
  DCHECK(messageLoop);
  base::test::ios::WaitUntilCondition(condition, messageLoop,
                                      base::TimeDelta::FromSeconds(10));
}

NSString* WebTestWithWebController::EvaluateJavaScriptAsString(
    NSString* script) {
  __block base::scoped_nsobject<NSString> evaluationResult;
  [webController_ evaluateJavaScript:script
                 stringResultHandler:^(NSString* result, NSError*) {
                   DCHECK([result isKindOfClass:[NSString class]]);
                   evaluationResult.reset([result copy]);
                 }];
  base::test::ios::WaitUntilCondition(^bool() {
    return evaluationResult;
  });
  return [[evaluationResult retain] autorelease];
}

id WebTestWithWebController::ExecuteJavaScript(NSString* script) {
  __block base::scoped_nsprotocol<id> executionResult;
  __block bool executionCompleted = false;
  [webController_ executeJavaScript:script
                  completionHandler:^(id result, NSError*) {
                    executionResult.reset([result copy]);
                    executionCompleted = true;
                  }];
  base::test::ios::WaitUntilCondition(^{
    return executionCompleted;
  });
  return [[executionResult retain] autorelease];
}

void WebTestWithWebController::WillProcessTask(
    const base::PendingTask& pending_task) {
  // Nothing to do.
}

void WebTestWithWebController::DidProcessTask(
    const base::PendingTask& pending_task) {
  processed_a_task_ = true;
}

bool WebTestWithWebController::ResetPageIfNavigationStalled(
    NSString* load_check) {
  NSString* inner_html = EvaluateJavaScriptAsString(
      @"(document && document.body && document.body.innerHTML) || 'undefined'");
  if ([inner_html rangeOfString:load_check].location == NSNotFound) {
    web_state_impl_->SetWebUsageEnabled(false);
    web_state_impl_->SetWebUsageEnabled(true);
    [webController_ triggerPendingLoad];
    return true;
  }
  return false;
}

NSString* WebTestWithWebController::CreateLoadCheck() {
  return [NSString stringWithFormat:@"<p style=\"display: none;\">%d</p>",
                                    s_html_load_count++];
}

}  // namespace web
