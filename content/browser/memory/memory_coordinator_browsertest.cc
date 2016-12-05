// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/common/content_features.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

class TestMemoryCoordinatorDelegate : public MemoryCoordinatorDelegate {
 public:
  TestMemoryCoordinatorDelegate() {}
  ~TestMemoryCoordinatorDelegate() override {}

  bool CanSuspendBackgroundedRenderer(int render_process_id) override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMemoryCoordinatorDelegate);
};

class MemoryCoordinatorTest : public ContentBrowserTest {
 public:
  MemoryCoordinatorTest() {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kMemoryCoordinator);
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorTest);
};

// TODO(bashi): Enable these tests on macos when MemoryMonitorMac is
// implemented.
#if !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(MemoryCoordinatorTest, HandleAdded) {
  GURL url = GetTestUrl("", "simple_page.html");
  NavigateToURL(shell(), url);
  EXPECT_EQ(1u, MemoryCoordinator::GetInstance()->children().size());
}

IN_PROC_BROWSER_TEST_F(MemoryCoordinatorTest, CanSuspendRenderer) {
  GURL url = GetTestUrl("", "simple_page.html");
  NavigateToURL(shell(), url);
  auto* memory_coordinator = MemoryCoordinator::GetInstance();
  memory_coordinator->SetDelegateForTesting(
      base::MakeUnique<TestMemoryCoordinatorDelegate>());
  EXPECT_EQ(1u, memory_coordinator->children().size());
  int render_process_id = memory_coordinator->children().begin()->first;
  // Foreground tab cannot be suspended.
  EXPECT_FALSE(memory_coordinator->CanSuspendRenderer(render_process_id));
}

IN_PROC_BROWSER_TEST_F(MemoryCoordinatorTest, CanThrottleRenderer) {
  GURL url = GetTestUrl("", "simple_page.html");
  NavigateToURL(shell(), url);
  auto* memory_coordinator = MemoryCoordinator::GetInstance();
  memory_coordinator->SetDelegateForTesting(
      base::MakeUnique<TestMemoryCoordinatorDelegate>());
  EXPECT_EQ(1u, memory_coordinator->children().size());
  int render_process_id = memory_coordinator->children().begin()->first;
  // Foreground tab cannot be throttled.
  EXPECT_FALSE(memory_coordinator->CanThrottleRenderer(render_process_id));
}

#endif  // !defined(OS_MACOSX)

}  // namespace content
