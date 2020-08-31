// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}  // namespace content

// Mock TabStripModelDelegate.
class TestTabStripModelDelegate : public TabStripModelDelegate {
 public:
  TestTabStripModelDelegate();
  ~TestTabStripModelDelegate() override;

  // Overridden from TabStripModelDelegate:
  void AddTabAt(const GURL& url,
                int index,
                bool foregroud,
                base::Optional<tab_groups::TabGroupId> group) override;
  Browser* CreateNewStripWithContents(std::vector<NewStripContents> contentses,
                                      const gfx::Rect& window_bounds,
                                      bool maximize) override;
  void WillAddWebContents(content::WebContents* contents) override;
  int GetDragActions() const override;
  bool CanDuplicateContentsAt(int index) override;
  void DuplicateContentsAt(int index) override;
  void MoveToExistingWindow(const std::vector<int>& indices,
                            int browser_index) override;
  std::vector<base::string16> GetExistingWindowsForMoveMenu() const override;
  bool CanMoveTabsToWindow(const std::vector<int>& indices) override;
  void MoveTabsToNewWindow(const std::vector<int>& indices) override;
  void MoveGroupToNewWindow(const tab_groups::TabGroupId& group) override;
  void CreateHistoricalTab(content::WebContents* contents) override;
  bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override;
  bool ShouldDisplayFavicon(content::WebContents* web_contents) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTabStripModelDelegate);
};

#endif  // CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_
