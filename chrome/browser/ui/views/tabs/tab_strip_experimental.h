// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_EXPERIMENTAL_H_

#include "chrome/browser/ui/tabs/tab_strip_model_experimental_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view_model.h"

class TabExperimental;
class TabStripModel;
class TabStripModelExperimental;

class TabStripExperimental : public TabStrip,
                             public TabStripModelExperimentalObserver {
 public:
  explicit TabStripExperimental(TabStripModel* model);
  ~TabStripExperimental() override;

  // views::View implementation:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  // TabStrip implementation:
  TabStripImpl* AsTabStripImpl() override;
  int GetMaxX() const override;
  void SetBackgroundOffset(const gfx::Point& offset) override;
  bool IsRectInWindowCaption(const gfx::Rect& rect) override;
  bool IsPositionInWindowCaption(const gfx::Point& point) override;
  bool IsTabStripCloseable() const override;
  bool IsTabStripEditable() const override;
  bool IsTabCrashed(int tab_index) const override;
  bool TabHasNetworkError(int tab_index) const override;
  TabAlertState GetTabAlertState(int tab_index) const override;
  void UpdateLoadingAnimations() override;

  // TabStripModelExperimentalObserver implementation:
  void TabInsertedAt(int index) override;
  void TabClosedAt(int index) override;
  void TabChangedAt(int index,
                    const TabDataExperimental& data,
                    TabStripModelObserver::TabChangeType type) override;

 private:
  // Non-owning.
  TabStripModelExperimental* model_;

  views::ViewModelT<TabExperimental> tabs_;

  views::BoundsAnimator bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(TabStripExperimental);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_EXPERIMENTAL_H_
