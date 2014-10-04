// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

class GURL;

namespace chrome {

class BrowserTabStripModelDelegate : public TabStripModelDelegate {
 public:
  explicit BrowserTabStripModelDelegate(Browser* browser);
  virtual ~BrowserTabStripModelDelegate();

 private:
  // Overridden from TabStripModelDelegate:
  virtual void AddTabAt(const GURL& url,
                           int index,
                           bool foreground) override;
  virtual Browser* CreateNewStripWithContents(
      const std::vector<NewStripContents>& contentses,
      const gfx::Rect& window_bounds,
      bool maximize) override;
  virtual void WillAddWebContents(content::WebContents* contents) override;
  virtual int GetDragActions() const override;
  virtual bool CanDuplicateContentsAt(int index) override;
  virtual void DuplicateContentsAt(int index) override;
  virtual void CreateHistoricalTab(content::WebContents* contents) override;
  virtual bool RunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  virtual bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  virtual bool CanBookmarkAllTabs() const override;
  virtual void BookmarkAllTabs() override;
  virtual RestoreTabType GetRestoreTabType() override;
  virtual void RestoreTab() override;

  void CloseFrame();

  Browser* browser_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<BrowserTabStripModelDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripModelDelegate);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
