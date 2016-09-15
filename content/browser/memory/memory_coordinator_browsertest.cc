// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/common/content_features.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

namespace {

void EnableForTesting() {
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(features::kMemoryCoordinator.name,
                                          "");
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace

class MemoryCoordinatorTest : public ContentBrowserTest {
 public:
  MemoryCoordinatorTest() {}

  void SetUp() override {
    EnableForTesting();
    ContentBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorTest);
};

IN_PROC_BROWSER_TEST_F(MemoryCoordinatorTest, HandleAdded) {
  GURL url = GetTestUrl("", "simple_page.html");
  NavigateToURL(shell(), url);
  EXPECT_EQ(1u, MemoryCoordinator::GetInstance()->NumChildrenForTesting());
}

}  // namespace content
