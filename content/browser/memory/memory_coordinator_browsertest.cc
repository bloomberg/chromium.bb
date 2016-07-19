// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/browser/memory_coordinator.h"
#include "components/memory_coordinator/common/memory_coordinator_features.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

class MemoryCoordinatorTest : public ContentBrowserTest {
 public:
  MemoryCoordinatorTest() {}

  void SetUp() override {
    memory_coordinator::EnableForTesting();
    ContentBrowserTest::SetUp();
  }

 protected:
  memory_coordinator::MemoryCoordinator* memory_coordinator() {
    return BrowserMainLoop::GetInstance()->memory_coordinator();
  }

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorTest);
};

IN_PROC_BROWSER_TEST_F(MemoryCoordinatorTest, HandleAdded) {
  GURL url = GetTestUrl("", "simple_page.html");
  NavigateToURL(shell(), url);
  EXPECT_EQ(1u, memory_coordinator()->NumChildrenForTesting());
}

}  // namespace content
