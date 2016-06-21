// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/base64.h"
#include "base/test/ios/wait_util.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

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

static int s_html_load_count;

void WebTestWithWebState::SetUp() {
  WebTest::SetUp();
  std::unique_ptr<WebStateImpl> web_state(new WebStateImpl(GetBrowserState()));
  web_state->GetNavigationManagerImpl().InitializeSession(nil, nil, NO, 0);
  web_state->SetWebUsageEnabled(true);
  web_state_.reset(web_state.release());

  // Force generation of child views; necessary for some tests.
  [GetWebController(web_state_.get()) triggerPendingLoad];
  s_html_load_count = 0;
}

void WebTestWithWebState::TearDown() {
  web_state_.reset();
  WebTest::TearDown();
}

void WebTestWithWebState::WillProcessTask(
    const base::PendingTask& pending_task) {
  // Nothing to do.
}

void WebTestWithWebState::DidProcessTask(
    const base::PendingTask& pending_task) {
  processed_a_task_ = true;
}

void WebTestWithWebState::LoadHtml(NSString* html) {
  LoadHtml([html UTF8String]);
}

void WebTestWithWebState::LoadHtml(const std::string& html) {
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

void WebTestWithWebState::LoadURL(const GURL& url) {
  // First step is to ensure that the web controller has finished any previous
  // page loads so the new load is not confused.
  while ([GetWebController(web_state()) loadPhase] != PAGE_LOADED)
    WaitForBackgroundTasks();
  id originalMockDelegate =
      [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
  id mockDelegate =
      [[WebDelegateMock alloc] initWithRepresentedObject:originalMockDelegate];
  id existingDelegate = GetWebController(web_state()).delegate;
  GetWebController(web_state()).delegate = mockDelegate;

  web::NavigationManagerImpl& navManager =
      [GetWebController(web_state()) webStateImpl]->GetNavigationManagerImpl();
  navManager.InitializeSession(@"name", nil, NO, 0);
  [navManager.GetSessionController() addPendingEntry:url
                                            referrer:web::Referrer()
                                          transition:ui::PAGE_TRANSITION_TYPED
                                   rendererInitiated:NO];

  [GetWebController(web_state()) loadCurrentURL];
  while ([GetWebController(web_state()) loadPhase] != PAGE_LOADED)
    WaitForBackgroundTasks();
  GetWebController(web_state()).delegate = existingDelegate;
  [web_state()->GetView() layoutIfNeeded];
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
    messageLoop->RunUntilIdle();
    if (processed_a_task_)  // Set in TaskObserver method.
      activitySeen = true;

  } while (activitySeen);
  messageLoop->RemoveTaskObserver(this);
}

void WebTestWithWebState::WaitForCondition(ConditionBlock condition) {
  base::MessageLoop* messageLoop = base::MessageLoop::current();
  DCHECK(messageLoop);
  base::test::ios::WaitUntilCondition(condition, messageLoop,
                                      base::TimeDelta::FromSeconds(10));
}

NSString* WebTestWithWebState::EvaluateJavaScriptAsString(NSString* script) {
  __block base::scoped_nsobject<NSString> evaluationResult;
  [GetWebController(web_state())
       evaluateJavaScript:script
      stringResultHandler:^(NSString* result, NSError*) {
        DCHECK([result isKindOfClass:[NSString class]]);
        evaluationResult.reset([result copy]);
      }];
  base::test::ios::WaitUntilCondition(^bool() {
    return evaluationResult;
  });
  return [[evaluationResult retain] autorelease];
}

id WebTestWithWebState::ExecuteJavaScript(NSString* script) {
  __block base::scoped_nsprotocol<id> executionResult;
  __block bool executionCompleted = false;
  [GetWebController(web_state()) executeJavaScript:script
                                 completionHandler:^(id result, NSError*) {
                                   executionResult.reset([result copy]);
                                   executionCompleted = true;
                                 }];
  base::test::ios::WaitUntilCondition(^{
    return executionCompleted;
  });
  return [[executionResult retain] autorelease];
}

web::WebState* WebTestWithWebState::web_state() {
  return web_state_.get();
}

const web::WebState* WebTestWithWebState::web_state() const {
  return web_state_.get();
}

bool WebTestWithWebState::ResetPageIfNavigationStalled(NSString* load_check) {
  NSString* inner_html = EvaluateJavaScriptAsString(
      @"(document && document.body && document.body.innerHTML) || 'undefined'");
  if ([inner_html rangeOfString:load_check].location == NSNotFound) {
    web_state_->SetWebUsageEnabled(false);
    web_state_->SetWebUsageEnabled(true);
    [GetWebController(web_state()) triggerPendingLoad];
    return true;
  }
  return false;
}

NSString* WebTestWithWebState::CreateLoadCheck() {
  return [NSString stringWithFormat:@"<p style=\"display: none;\">%d</p>",
                                    s_html_load_count++];
}

}  // namespace web
