// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#define CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class TabContentsWrapper;

namespace printing {

// Manages hidden tabs that prints documents in the background.
// The hidden tabs are no longer part of any Browser / TabStripModel.
// They get deleted when the tab finishes printing.
class BackgroundPrintingManager : public base::NonThreadSafe,
                                  public NotificationObserver {
 public:
  typedef std::set<TabContentsWrapper*> TabContentsWrapperSet;

  BackgroundPrintingManager();
  virtual ~BackgroundPrintingManager();

  // Takes ownership of |preview_tab| and deletes it when |preview_tab| finishes
  // printing. This removes the TabContentsWrapper from its TabStrip and
  // hides it from the user.
  void OwnPrintPreviewTab(TabContentsWrapper* preview_tab);

  // Let others iterate over the list of background printing tabs.
  TabContentsWrapperSet::const_iterator begin();
  TabContentsWrapperSet::const_iterator end();

  // Returns true if |printing_tabs_| contains |preview_tab|.
  bool HasPrintPreviewTab(TabContentsWrapper* preview_tab);

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // The set of print preview tabs managed by BackgroundPrintingManager.
  TabContentsWrapperSet printing_tabs_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPrintingManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
