// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/base/models/list_selection_model.h"

class TabStrip;

class FakeBaseTabStripController : public TabStripController {
 public:
  FakeBaseTabStripController();
  virtual ~FakeBaseTabStripController();

  void AddTab(int index, bool is_active);
  void RemoveTab(int index);

  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }

  // TabStripController overrides:
  virtual const ui::ListSelectionModel& GetSelectionModel() override;
  virtual int GetCount() const override;
  virtual bool IsValidIndex(int index) const override;
  virtual bool IsActiveTab(int index) const override;
  virtual int GetActiveIndex() const override;
  virtual bool IsTabSelected(int index) const override;
  virtual bool IsTabPinned(int index) const override;
  virtual bool IsNewTabPage(int index) const override;
  virtual void SelectTab(int index) override;
  virtual void ExtendSelectionTo(int index) override;
  virtual void ToggleSelected(int index) override;
  virtual void AddSelectionFromAnchorTo(int index) override;
  virtual void CloseTab(int index, CloseTabSource source) override;
  virtual void ToggleTabAudioMute(int index) override;
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) override;
  virtual void UpdateLoadingAnimations() override;
  virtual int HasAvailableDragActions() const override;
  virtual void OnDropIndexUpdate(int index, bool drop_before) override;
  virtual void PerformDrop(bool drop_before,
                           int index,
                           const GURL& url) override;
  virtual bool IsCompatibleWith(TabStrip* other) const override;
  virtual void CreateNewTab() override;
  virtual void CreateNewTabWithLocation(const base::string16& loc) override;
  virtual bool IsIncognito() override;
  virtual void StackedLayoutMaybeChanged() override;
  virtual void OnStartedDraggingTabs() override;
  virtual void OnStoppedDraggingTabs() override;
  virtual void CheckFileSupported(const GURL& url) override;

 private:
  TabStrip* tab_strip_;

  int num_tabs_;
  int active_index_;

  ui::ListSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
