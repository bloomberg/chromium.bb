// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_OBSERVER_H_

#include "chrome/browser/ui/tabs/tab_change_type.h"

class TabDataExperimental;

class TabStripModelExperimentalObserver {
 public:
  // If the parent is null the index is into the toplevel tab strip.
  virtual void TabInserted(const TabDataExperimental* data, bool is_active) = 0;
  virtual void TabClosing(const TabDataExperimental* data) = 0;

  // TODO(brettw) need a way to specify what changed so the tab doesn't need
  // to redraw everything.
  virtual void TabChanged(const TabDataExperimental* data,
                          TabChangeType change_type) = 0;

  // TODO(brettw) Need to add support for multi-selection. We probably don't
  // want to expose a ListSelectionModel here because of the mismatch between
  // indices and tabs. Probably we want a vector of selected tabs or something.
  //
  // |old_active| will be null for the first call.
  virtual void TabSelectionChanged(const TabDataExperimental* old_active,
                                   const TabDataExperimental* new_active) = 0;

 private:
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_OBSERVER_H_
