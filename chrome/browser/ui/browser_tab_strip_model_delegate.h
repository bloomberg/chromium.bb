// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

class Browser;
class DockInfo;
class GURL;
class TabContents;

namespace content {
struct Referrer;
class SiteInstance;
}

namespace gfx {
class Rect;
}

namespace chrome {

class BrowserTabStripModelDelegate : public TabStripModelDelegate {
 public:
  explicit BrowserTabStripModelDelegate(Browser* browser);
  virtual ~BrowserTabStripModelDelegate();

 private:
  // Overridden from TabStripModelDelegate:
  virtual TabContents* AddBlankTab(bool foreground) OVERRIDE;
  virtual TabContents* AddBlankTabAt(int index, bool foreground) OVERRIDE;
  virtual Browser* CreateNewStripWithContents(
      TabContents* detached_contents,
      const gfx::Rect& window_bounds,
      const DockInfo& dock_info,
      bool maximize) OVERRIDE;
  virtual int GetDragActions() const OVERRIDE;
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      const content::Referrer& referrer,
      Profile* profile,
      content::PageTransition transition,
      bool defer_load,
      content::SiteInstance* instance) const OVERRIDE;
  virtual bool CanDuplicateContentsAt(int index) OVERRIDE;
  virtual void DuplicateContentsAt(int index) OVERRIDE;
  virtual void CloseFrameAfterDragSession() OVERRIDE;
  virtual void CreateHistoricalTab(TabContents* contents) OVERRIDE;
  virtual bool RunUnloadListenerBeforeClosing(TabContents* contents) OVERRIDE;
  virtual bool CanBookmarkAllTabs() const OVERRIDE;
  virtual void BookmarkAllTabs() OVERRIDE;
  virtual bool CanRestoreTab() OVERRIDE;
  virtual void RestoreTab() OVERRIDE;

  void CloseFrame();

  Browser* browser_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<BrowserTabStripModelDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripModelDelegate);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
