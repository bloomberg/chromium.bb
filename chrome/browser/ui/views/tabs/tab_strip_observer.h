// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_OBSERVER_H_

#include "chrome/browser/ui/views/chrome_views_export.h"

class TabStrip;

////////////////////////////////////////////////////////////////////////////////
//
// TabStripObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStrip.
//
//  Register your TabStripObserver with the TabStrip using its
//  Add/RemoveObserver methods.
//
////////////////////////////////////////////////////////////////////////////////
class CHROME_VIEWS_EXPORT TabStripObserver {
 public:
  // A new tab was added to |tab_strip| at |index|.
  virtual void TabStripAddedTabAt(TabStrip* tab_strip, int index);

  // The tab at |from_index| was moved to |to_index| in |tab_strip|.
  virtual void TabStripMovedTab(TabStrip* tab_strip,
                                int from_index,
                                int to_index);

  // The tab at |index| was removed from |tab_strip|.
  virtual void TabStripRemovedTabAt(TabStrip* tab_strip, int index);

  // Sent when the |tabstrip| is about to be deleted and any reference held must
  // be dropped.
  virtual void TabStripDeleted(TabStrip* tab_strip);

 protected:
  virtual ~TabStripObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_OBSERVER_H_
