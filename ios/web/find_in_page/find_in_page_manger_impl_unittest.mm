// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/public/test/fakes/fake_find_in_page_manager_delegate.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Frame ids of fake web frames used in this test class.
const char kOneMatchFrameId[] = "frame_with_one_match";
const char kTwoMatchesFrameId[] = "frame_with_two_matches";

}  // namespace

namespace web {

// Tests FindInPageManagerImpl and verifies that the state of
// FindInPageManagerDelegate is correct depending on what web frames return.
class FindInPageManagerImplTest : public WebTest {
 protected:
  FindInPageManagerImplTest()
      : test_web_state_(std::make_unique<TestWebState>()) {
    test_web_state_->CreateWebFramesManager();
    FindInPageManagerImpl::CreateForWebState(test_web_state_.get());
    GetFindInPageManager()->SetDelegate(&fake_delegate_);
  }

  // Returns the FindInPageManager associated with |test_web_state_|.
  FindInPageManager* GetFindInPageManager() {
    return FindInPageManager::FromWebState(test_web_state_.get());
  }
  // Returns a FakeWebFrame with id |frame_id| that will return |js_result| for
  // the function call "findInString.findString".
  std::unique_ptr<FakeWebFrame> CreateWebFrameWithJsResultForFind(
      std::unique_ptr<base::Value> js_result,
      const std::string& frame_id) {
    auto frame_with_one_match =
        std::make_unique<FakeWebFrame>(frame_id,
                                       /*is_main_frame=*/false, GURL());
    frame_with_one_match->AddJsResultForFunctionCall(std::move(js_result),
                                                     kFindInPageSearch);
    return frame_with_one_match;
  }

  std::unique_ptr<TestWebState> test_web_state_;
  FakeFindInPageManagerDelegate fake_delegate_;
};

// Tests that Find In Page responds with a total match count of three when a
// frame has one match and another frame has two matches.
TEST_F(FindInPageManagerImplTest, FindMatchesMultipleFrames) {
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));
  test_web_state_->AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->last_javascript_call());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(3, fake_delegate_.state()->match_count);
}

// Tests that Find In Page responds with a total match count of one when a frame
// has one match but find in one frame was cancelled. This can occur if the
// frame becomes unavailable.
TEST_F(FindInPageManagerImplTest, FrameCancelFind) {
  auto frame_with_null_result = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(), "frame");
  FakeWebFrame* frame_with_null_result_ptr = frame_with_null_result.get();
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  test_web_state_->AddWebFrame(std::move(frame_with_null_result));
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_null_result_ptr->last_javascript_call());
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(1, fake_delegate_.state()->match_count);
}

// Tests that Find In Page returns a total match count matching the latest find
// if two finds are called.
TEST_F(FindInPageManagerImplTest, ReturnLatestFind) {
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));
  test_web_state_->AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->last_javascript_call());
  test_web_state_->RemoveWebFrame(kOneMatchFrameId);
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(2, fake_delegate_.state()->match_count);
}

// Tests that Find In Page should not return if the web state is destroyed
// during a find.
TEST_F(FindInPageManagerImplTest, DestroyWebStateDuringFind) {
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  test_web_state_ = nullptr;
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find In Page updates total match count when a frame with matches
// becomes unavailable during find.
TEST_F(FindInPageManagerImplTest, FrameUnavailableAfterDelegateCallback) {
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));
  test_web_state_->AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->last_javascript_call());
  test_web_state_->RemoveWebFrame(kTwoMatchesFrameId);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(1, fake_delegate_.state()->match_count);
}

// Tests that Find In Page returns with the right match count for a frame with
// one match and another that requires pumping to return its two matches.
TEST_F(FindInPageManagerImplTest, FrameRespondsWithPending) {
  std::unique_ptr<FakeWebFrame> frame_with_two_matches =
      CreateWebFrameWithJsResultForFind(std::make_unique<base::Value>(-1.0),
                                        kTwoMatchesFrameId);
  frame_with_two_matches->AddJsResultForFunctionCall(
      std::make_unique<base::Value>(2.0), kFindInPagePump);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  test_web_state_->AddWebFrame(std::move(frame_with_two_matches));
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->last_javascript_call());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ("__gCrWeb.findInPage.pumpSearch(100.0);",
            frame_with_two_matches_ptr->last_javascript_call());
  EXPECT_EQ(3, fake_delegate_.state()->match_count);
}

// Tests that Find In Page doesn't fail when delegate is not set.
TEST_F(FindInPageManagerImplTest, DelegateNotSet) {
  GetFindInPageManager()->SetDelegate(nullptr);
  auto frame_with_one_match = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  test_web_state_->AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->last_javascript_call());
  base::RunLoop().RunUntilIdle();
}

// Tests that Find In Page returns no matches if can't call JavaScript function.
TEST_F(FindInPageManagerImplTest, FrameCannotCallJavaScriptFunction) {
  auto frame_cannot_call_func = CreateWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  frame_cannot_call_func->set_can_call_function(false);
  test_web_state_->AddWebFrame(std::move(frame_cannot_call_func));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->match_count);
}

// Tests that  Find In Page responds with a total match count of zero when there
// are no known webpage frames.
TEST_F(FindInPageManagerImplTest, NoFrames) {
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->match_count);
}

}  // namespace web
