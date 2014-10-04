// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

// Mock TabStripModelDelegate.
class TestTabStripModelDelegate : public TabStripModelDelegate {
 public:
  TestTabStripModelDelegate();
  virtual ~TestTabStripModelDelegate();

  // Overridden from TabStripModelDelegate:
  virtual void AddTabAt(const GURL& url, int index, bool foregroud) override;
  virtual Browser* CreateNewStripWithContents(
      const std::vector<NewStripContents>& contentses,
      const gfx::Rect& window_bounds,
      bool maximize) override;
  virtual void WillAddWebContents(content::WebContents* contents) override;
  virtual int GetDragActions() const override;
  virtual bool CanDuplicateContentsAt(int index) override;
  virtual void DuplicateContentsAt(int index) override;
  virtual void CreateHistoricalTab(content::WebContents* contents) override;
  virtual bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  virtual bool RunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  virtual RestoreTabType GetRestoreTabType() override;
  virtual void RestoreTab() override;
  virtual bool CanBookmarkAllTabs() const override;
  virtual void BookmarkAllTabs() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTabStripModelDelegate);
};

#endif  // CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_
