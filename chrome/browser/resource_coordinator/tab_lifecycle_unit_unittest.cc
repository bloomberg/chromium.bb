// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include "base/macros.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class DummyTabLifecycleUnit : public TabLifecycleUnit {
 public:
  explicit DummyTabLifecycleUnit(content::WebContents* contents)
      : contents_(contents) {
    TabLifecycleUnit::SetForWebContents(contents_, this);
  }

  ~DummyTabLifecycleUnit() override {
    TabLifecycleUnit::SetForWebContents(contents_, nullptr);
  }

  // TabLifecycleUnit:
  content::WebContents* GetWebContents() const override { return nullptr; }
  content::RenderProcessHost* GetRenderProcessHost() const override {
    return nullptr;
  }
  bool IsMediaTab() const override { return false; }
  bool IsAutoDiscardable() const override { return false; }
  void SetAutoDiscardable(bool) override {}

  // LifecycleUnit:
  base::string16 GetTitle() const override { return base::string16(); }
  std::string GetIconURL() const override { return std::string(); }
  SortKey GetSortKey() const override { return SortKey(); }
  State GetState() const override { return State::LOADED; }
  int GetEstimatedMemoryFreedOnDiscardKB() const override { return 0; }
  bool CanDiscard(DiscardReason) const override { return false; }
  bool Discard(DiscardReason) override { return false; }

 private:
  content::WebContents* const contents_;

  DISALLOW_COPY_AND_ASSIGN(DummyTabLifecycleUnit);
};

}  // namespace

using TabLifecycleUnitTest = ChromeRenderViewHostTestHarness;

TEST_F(TabLifecycleUnitTest, FromWebContentsNullByDefault) {
  EXPECT_FALSE(TabLifecycleUnit::FromWebContents(web_contents()));
}

TEST_F(TabLifecycleUnitTest, SetForWebContents) {
  {
    DummyTabLifecycleUnit tab_lifecycle_unit(web_contents());
    EXPECT_EQ(&tab_lifecycle_unit,
              TabLifecycleUnit::FromWebContents(web_contents()));
  }
  EXPECT_FALSE(TabLifecycleUnit::FromWebContents(web_contents()));
  {
    DummyTabLifecycleUnit tab_lifecycle_unit(web_contents());
    EXPECT_EQ(&tab_lifecycle_unit,
              TabLifecycleUnit::FromWebContents(web_contents()));
  }
  EXPECT_FALSE(TabLifecycleUnit::FromWebContents(web_contents()));
}

}  // namespace resource_coordinator
