// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"

#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

class TabStripModelExperimentalTest : public ChromeRenderViewHostTestHarness {
 public:
  TabStripModelExperimentalTest() = default;

  TabStripModelExperimental& model() { return *model_; }

  // Provides internal access to check the tabs.
  const std::vector<std::unique_ptr<TabDataExperimental>>& tabs() const {
    return model_->tabs_;
  }

  // testing::Test overrides.
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    model_ = std::make_unique<TabStripModelExperimental>(&delegate_, profile());
  }

  WebContents* CreateWebContents() {
    return WebContents::Create(WebContents::CreateParams(profile()));
  }

  // Appends the given data (indicating a tab, etc.) to the model using the
  // internal API. This bypasses any default grouping logic.
  void AppendData(std::unique_ptr<TabDataExperimental> data) {
    model_->tabs_.push_back(std::move(data));
    model_->UpdateViewCount();
  }

 private:
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModelExperimental> model_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelExperimentalTest);
};

// This test creates the following model with the following view indices.
//
//  0: WebContents
//     Group
//  1: Hub group
//  2:   Child
//  3:   Child
//     Group
//  4:   Child
//  5:   Child
TEST_F(TabStripModelExperimentalTest, ViewIterator) {
  // View index 0: Single WebContents.
  std::unique_ptr<WebContents> contents0(CreateWebContents());
  AppendData(std::make_unique<TabDataExperimental>(
      nullptr, TabDataExperimental::Type::kSingle, contents0.get(), &model()));

  // Empty group (no view index).
  AppendData(std::make_unique<TabDataExperimental>(
      nullptr, TabDataExperimental::Type::kGroup, nullptr, &model()));

  // View indices 1, 2, and 3: Hub group with two children.
  std::unique_ptr<WebContents> contents1(CreateWebContents());
  std::unique_ptr<WebContents> contents2(CreateWebContents());
  std::unique_ptr<WebContents> contents3(CreateWebContents());
  auto hub = std::make_unique<TabDataExperimental>(
      nullptr, TabDataExperimental::Type::kHubAndSpoke, contents1.get(),
      &model());
  hub->children().push_back(std::make_unique<TabDataExperimental>(
      hub.get(), TabDataExperimental::Type::kSingle, contents2.get(),
      &model()));
  hub->children().push_back(std::make_unique<TabDataExperimental>(
      hub.get(), TabDataExperimental::Type::kSingle, contents3.get(),
      &model()));
  AppendData(std::move(hub));

  // Group (no view index) with two children (view indices 4 and 5).
  std::unique_ptr<WebContents> contents4(CreateWebContents());
  std::unique_ptr<WebContents> contents5(CreateWebContents());
  auto group = std::make_unique<TabDataExperimental>(
      nullptr, TabDataExperimental::Type::kGroup, nullptr, &model());
  group->children().push_back(std::make_unique<TabDataExperimental>(
      group.get(), TabDataExperimental::Type::kSingle, contents4.get(),
      &model()));
  group->children().push_back(std::make_unique<TabDataExperimental>(
      group.get(), TabDataExperimental::Type::kSingle, contents5.get(),
      &model()));
  AppendData(std::move(group));

  EXPECT_FALSE(model().empty());
  EXPECT_EQ(6, model().count());

  // Expect this sequence of stuff in the model when iterating.
  std::vector<std::pair<TabDataExperimental::Type, WebContents*>> expected;
  expected.emplace_back(TabDataExperimental::Type::kSingle, contents0.get());
  expected.emplace_back(TabDataExperimental::Type::kHubAndSpoke,
                        contents1.get());
  expected.emplace_back(TabDataExperimental::Type::kSingle, contents2.get());
  expected.emplace_back(TabDataExperimental::Type::kSingle, contents3.get());
  expected.emplace_back(TabDataExperimental::Type::kSingle, contents4.get());
  expected.emplace_back(TabDataExperimental::Type::kSingle, contents5.get());

  TabStripModelExperimental::ViewIterator iter = model().begin();
  int index = 0;
  while (iter != model().end() && index < static_cast<int>(expected.size())) {
    EXPECT_EQ(expected[index].first, iter->type());
    EXPECT_EQ(expected[index].second, iter->contents());

    // Test both types of increment on the iterator.
    if (index % 2)
      iter++;
    else
      ++iter;
    index++;
  }

  // Should be at the end.
  EXPECT_EQ(model().end(), iter);
  EXPECT_EQ(6, index);

  // Go back the opposite way.
  do {
    // Test both types of decrement on the iterator.
    if (index % 2)
      iter--;
    else
      --iter;
    index--;

    EXPECT_EQ(expected[index].first, iter->type());
    EXPECT_EQ(expected[index].second, iter->contents());
  } while (iter != model().begin() && index > 0);

  // Should be at the beginning.
  EXPECT_EQ(model().begin(), iter);
  EXPECT_EQ(0, index);
}
