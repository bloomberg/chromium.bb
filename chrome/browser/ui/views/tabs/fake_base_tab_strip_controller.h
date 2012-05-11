// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/tabs/tab_strip_selection_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"

class TabStrip;

class FakeBaseTabStripController : public TabStripController {
 public:
  FakeBaseTabStripController();
  virtual ~FakeBaseTabStripController();

  void AddTab(int index);
  void RemoveTab(int index);

  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }

  // TabStripController overrides:
  virtual const TabStripSelectionModel& GetSelectionModel() OVERRIDE;
  virtual int GetCount() const OVERRIDE;
  virtual bool IsValidIndex(int index) const OVERRIDE;
  virtual bool IsActiveTab(int index) const OVERRIDE;
  virtual int GetActiveIndex() const OVERRIDE;
  virtual bool IsTabSelected(int index) const OVERRIDE;
  virtual bool IsTabPinned(int index) const OVERRIDE;
  virtual bool IsTabCloseable(int index) const OVERRIDE;
  virtual bool IsNewTabPage(int index) const OVERRIDE;
  virtual void SelectTab(int index) OVERRIDE;
  virtual void ExtendSelectionTo(int index) OVERRIDE;
  virtual void ToggleSelected(int index) OVERRIDE;
  virtual void AddSelectionFromAnchorTo(int index) OVERRIDE;
  virtual void CloseTab(int index) OVERRIDE;
  virtual void ShowContextMenuForTab(BaseTab* tab,
                                     const gfx::Point& p) OVERRIDE;
  virtual void UpdateLoadingAnimations() OVERRIDE;
  virtual int HasAvailableDragActions() const OVERRIDE;
  virtual void OnDropIndexUpdate(int index, bool drop_before) OVERRIDE;
  virtual void PerformDrop(bool drop_before,
                           int index,
                           const GURL& url) OVERRIDE;
  virtual bool IsCompatibleWith(TabStrip* other) const OVERRIDE;
  virtual void CreateNewTab() OVERRIDE;
  virtual void ClickActiveTab(int index) OVERRIDE;
  virtual bool IsIncognito() OVERRIDE;

 private:
  TabStrip* tab_strip_;

  int num_tabs_;

  TabStripSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
