// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/content_capture/browser/content_capture_receiver_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_capture {
namespace {

// Fake ContentCaptureSender to call ContentCaptureReceiver mojom interface.
class FakeContentCaptureSender {
 public:
  FakeContentCaptureSender() {}

  void DidCaptureContent(const ContentCaptureData& captured_content,
                         bool first_data) {
    content_capture_receiver_->DidCaptureContent(captured_content, first_data);
  }

  void DidRemoveContent(const std::vector<int64_t>& data) {
    content_capture_receiver_->DidRemoveContent(data);
  }

  mojom::ContentCaptureReceiverAssociatedRequest GetAssociatedRequest() {
    return mojo::MakeRequestAssociatedWithDedicatedPipe(
        &content_capture_receiver_);
  }

 private:
  mojom::ContentCaptureReceiverAssociatedPtr content_capture_receiver_ =
      nullptr;
};

// The helper class implements ContentCaptureReceiverManager and keeps the
// result for verification.
class ContentCaptureReceiverManagerHelper
    : public ContentCaptureReceiverManager {
 public:
  static void Create(content::WebContents* web_contents) {
    new ContentCaptureReceiverManagerHelper(web_contents);
  }

  ContentCaptureReceiverManagerHelper(content::WebContents* web_contents)
      : ContentCaptureReceiverManager(web_contents) {}

  void DidCaptureContent(const ContentCaptureSession& parent_session,
                         const ContentCaptureData& data) override {
    parent_session_ = parent_session;
    captured_data_ = data;
  }

  void DidRemoveContent(const ContentCaptureSession& session,
                        const std::vector<int64_t>& ids) override {
    session_ = session;
    removed_ids_ = ids;
  }

  void DidRemoveSession(const ContentCaptureSession& data) override {
    removed_session_ = data;
  }

  const ContentCaptureSession& parent_session() const {
    return parent_session_;
  }

  const ContentCaptureSession& session() const { return session_; }

  const ContentCaptureData& captured_data() const { return captured_data_; }

  const ContentCaptureSession& removed_session() const {
    return removed_session_;
  }

  const std::vector<int64_t>& removed_ids() const { return removed_ids_; }

 private:
  ContentCaptureSession parent_session_;
  ContentCaptureSession session_;
  ContentCaptureData captured_data_;
  std::vector<int64_t> removed_ids_;
  ContentCaptureSession removed_session_;
};

class ContentCaptureReceiverTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ContentCaptureReceiverManagerHelper::Create(web_contents());
    content_capture_receiver_manager_helper_ =
        static_cast<ContentCaptureReceiverManagerHelper*>(
            ContentCaptureReceiverManager::FromWebContents(web_contents()));
    // This needed to keep the WebContentsObserverSanityChecker checks happy for
    // when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));
    content_capture_sender_ = std::make_unique<FakeContentCaptureSender>();
    main_frame_ = web_contents()->GetMainFrame();
    // Binds sender with receiver.
    ContentCaptureReceiverManager::BindContentCaptureReceiver(
        content_capture_sender_->GetAssociatedRequest(), main_frame_);

    ContentCaptureData child;
    // Have the unique id for text content.
    child.id = 2;
    child.value = base::ASCIIToUTF16("Hello");
    child.bounds = gfx::Rect(5, 5, 5, 5);
    // No need to set id in sender.
    test_data_.value = base::ASCIIToUTF16("http://foo.com/bar");
    test_data_.bounds = gfx::Rect(10, 10);
    test_data_.children.push_back(child);
    test_data2_.value = base::ASCIIToUTF16("http://foo.org/bar");
    test_data2_.bounds = gfx::Rect(10, 10);
    test_data2_.children.push_back(child);
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  void SetupChildFrame() {
    child_content_capture_sender_ =
        std::make_unique<FakeContentCaptureSender>();
    child_frame_ =
        content::RenderFrameHostTester::For(main_frame_)->AppendChild("child");
    // Binds sender with receiver for child frame.
    ContentCaptureReceiverManager::BindContentCaptureReceiver(
        child_content_capture_sender_->GetAssociatedRequest(), child_frame_);
  }

  FakeContentCaptureSender* content_capture_sender() {
    return content_capture_sender_.get();
  }

  FakeContentCaptureSender* child_content_capture_sender() {
    return child_content_capture_sender_.get();
  }

  const ContentCaptureData& test_data() const { return test_data_; }
  const ContentCaptureData& test_data2() const { return test_data2_; }
  const std::vector<int64_t>& expected_removed_ids() const {
    return expected_removed_ids_;
  }

  ContentCaptureData GetExpectedTestData(bool main_frame) const {
    ContentCaptureData expected(test_data_);
    // Replaces the id with expected id.
    expected.id = ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_
                                                               : child_frame_);
    return expected;
  }

  ContentCaptureData GetExpectedTestData2(bool main_frame) const {
    ContentCaptureData expected(test_data2_);
    // Replaces the id with expected id.
    expected.id = ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_
                                                               : child_frame_);
    return expected;
  }

  ContentCaptureReceiverManagerHelper* content_capture_receiver_manager_helper()
      const {
    return content_capture_receiver_manager_helper_;
  }

  void VerifySession(const ContentCaptureSession& expected,
                     const ContentCaptureSession& result) const {
    EXPECT_EQ(expected.size(), result.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(expected[i].id, result[i].id);
      EXPECT_EQ(expected[i].value, result[i].value);
      EXPECT_EQ(expected[i].bounds, result[i].bounds);
    }
  }

  void DidCaptureContent(const ContentCaptureData& captured_content,
                         bool first_data) {
    base::RunLoop run_loop;
    content_capture_sender()->DidCaptureContent(captured_content, first_data);
    run_loop.RunUntilIdle();
  }

  void DidCaptureContentForChildFrame(
      const ContentCaptureData& captured_content,
      bool first_data) {
    base::RunLoop run_loop;
    child_content_capture_sender()->DidCaptureContent(captured_content,
                                                      first_data);
    run_loop.RunUntilIdle();
  }

  void DidRemoveContent(const std::vector<int64_t>& data) {
    base::RunLoop run_loop;
    content_capture_sender()->DidRemoveContent(data);
    run_loop.RunUntilIdle();
  }

 protected:
  ContentCaptureReceiverManagerHelper*
      content_capture_receiver_manager_helper_ = nullptr;

 private:
  // The sender for main frame.
  std::unique_ptr<FakeContentCaptureSender> content_capture_sender_;
  // The sender for child frame.
  std::unique_ptr<FakeContentCaptureSender> child_content_capture_sender_;
  content::RenderFrameHost* main_frame_ = nullptr;
  content::RenderFrameHost* child_frame_ = nullptr;
  ContentCaptureData test_data_;
  ContentCaptureData test_data2_;
  // Expected removed Ids.
  std::vector<int64_t> expected_removed_ids_{2};
};

TEST_F(ContentCaptureReceiverTest, DidCaptureContent) {
  DidCaptureContent(test_data(), true /* first_data */);
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
}

TEST_F(ContentCaptureReceiverTest, DidCaptureContentWithUpdate) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
  // Simulates to update the content within the same document.
  DidCaptureContent(test_data2(), false /* first_data */);
  // Verifies to get test_data2() with correct frame content id.
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  // Verifies that the sesssion isn't removed.
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  EXPECT_EQ(GetExpectedTestData2(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
}

TEST_F(ContentCaptureReceiverTest, DidRemoveSession) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
  // Simulates to navigate other document.
  DidCaptureContent(test_data2(), true /* first_data */);
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  // Verifies that the previous session was removed.
  EXPECT_EQ(
      1u, content_capture_receiver_manager_helper()->removed_session().size());
  std::vector<ContentCaptureData> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected,
                content_capture_receiver_manager_helper()->removed_session());
  // Verifies that we get the test_data2() from the new document.
  EXPECT_EQ(GetExpectedTestData2(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
}

TEST_F(ContentCaptureReceiverTest, DidRemoveContent) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
  // Simulates to remove the content.
  DidRemoveContent(expected_removed_ids());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  // Verifies that the removed_ids() was removed from the correct session.
  EXPECT_EQ(expected_removed_ids(),
            content_capture_receiver_manager_helper()->removed_ids());
  std::vector<ContentCaptureData> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected, content_capture_receiver_manager_helper()->session());
}

TEST_F(ContentCaptureReceiverTest, ChildFrameDidCaptureContent) {
  // Simulate add child frame.
  SetupChildFrame();
  // Simulate to capture the content from main frame.
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
  // Simulate to capture the content from child frame.
  DidCaptureContentForChildFrame(test_data2(), true /* first_data */);
  // Verifies that the parent_session was set correctly.
  EXPECT_FALSE(
      content_capture_receiver_manager_helper()->parent_session().empty());
  std::vector<ContentCaptureData> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected,
                content_capture_receiver_manager_helper()->parent_session());
  EXPECT_TRUE(
      content_capture_receiver_manager_helper()->removed_session().empty());
  // Verifies that we receive the correct content from child frame.
  EXPECT_EQ(GetExpectedTestData2(false /* main_frame */),
            content_capture_receiver_manager_helper()->captured_data());
}

class ContentCaptureReceiverMultipleFrameTest
    : public ContentCaptureReceiverTest {
 public:
  void SetUp() override {
    // Setup multiple frames before creates ContentCaptureReceiverManager.
    content::RenderViewHostTestHarness::SetUp();
    // This needed to keep the WebContentsObserverSanityChecker checks happy for
    // when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));
    content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
        ->AppendChild("child");
    ContentCaptureReceiverManagerHelper::Create(web_contents());
    content_capture_receiver_manager_helper_ =
        static_cast<ContentCaptureReceiverManagerHelper*>(
            ContentCaptureReceiverManager::FromWebContents(web_contents()));
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }
};

TEST_F(ContentCaptureReceiverMultipleFrameTest,
       ReceiverCreatedForExistingFrame) {
  EXPECT_EQ(
      2u,
      content_capture_receiver_manager_helper()->GetFrameMapSizeForTesting());
}
}  // namespace
}  // namespace content_capture
