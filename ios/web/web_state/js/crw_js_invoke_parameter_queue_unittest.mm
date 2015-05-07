// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_invoke_parameter_queue.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

class CRWJSInvokeParameterQueueTest : public PlatformTest {
 public:
  CRWJSInvokeParameterQueueTest() {}
  ~CRWJSInvokeParameterQueueTest() override {}
  void SetUp() override;
  void TearDown() override;

 protected:
  base::scoped_nsobject<CRWJSInvokeParameterQueue> js_invoke_parameter_queue_;
};

void CRWJSInvokeParameterQueueTest::SetUp() {
  js_invoke_parameter_queue_.reset([[CRWJSInvokeParameterQueue alloc] init]);
}

void CRWJSInvokeParameterQueueTest::TearDown() {
  js_invoke_parameter_queue_.reset();
}

TEST_F(CRWJSInvokeParameterQueueTest, testRemoveFromEmptyQueue) {
  EXPECT_TRUE([js_invoke_parameter_queue_ isEmpty]);
  EXPECT_NSEQ(nil, [js_invoke_parameter_queue_ popInvokeParameters]);
}

TEST_F(CRWJSInvokeParameterQueueTest, testFifoOrdering) {
  GURL url1("http://url1.example.com/path1");
  GURL url2("https://url2.example.com/path2");
  EXPECT_TRUE([js_invoke_parameter_queue_ isEmpty]);
  [js_invoke_parameter_queue_ addCommandString:@"command1"
                             userIsInteracting:YES
                                     originURL:url1
                                   forWindowId:@"one"];
  [js_invoke_parameter_queue_ addCommandString:@"command2"
                             userIsInteracting:NO
                                     originURL:url2
                                   forWindowId:@"two"];
  EXPECT_FALSE([js_invoke_parameter_queue_ isEmpty]);

  CRWJSInvokeParameters* param1 =
      [js_invoke_parameter_queue_ popInvokeParameters];
  EXPECT_NSEQ(@"command1", [param1 commandString]);
  EXPECT_TRUE([param1 userIsInteracting]);
  EXPECT_EQ(url1, [param1 originURL]);
  EXPECT_NSEQ(@"one", [param1 windowId]);

  CRWJSInvokeParameters* param2 =
      [js_invoke_parameter_queue_ popInvokeParameters];
  EXPECT_NSEQ(@"command2", [param2 commandString]);
  EXPECT_FALSE([param2 userIsInteracting]);
  EXPECT_EQ(url2, [param2 originURL]);
  EXPECT_NSEQ(@"two", [param2 windowId]);

  EXPECT_TRUE([js_invoke_parameter_queue_ isEmpty]);
  EXPECT_NSEQ(nil, [js_invoke_parameter_queue_ popInvokeParameters]);
}

}  // namespace
