// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/browser_tab_strip_controller.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_menu_model.h"
#include "chrome/browser/views/tabs/side_tab_strip.h"
#include "views/controls/menu/menu_2.h"
#include "views/widget/widget.h"

class BrowserTabStripController::TabContextMenuContents
    : public menus::SimpleMenuModel::Delegate {
 public:
  TabContextMenuContents(int tab_index, BrowserTabStripController* controller)
      : ALLOW_THIS_IN_INITIALIZER_LIST(model_(this)),
        tab_index_(tab_index),
        controller_(controller) {
    Build();
  }
  virtual ~TabContextMenuContents() {
    menu_->CancelMenu();
  }

  void RunMenuAt(const gfx::Point& point) {
    menu_->RunMenuAt(point, views::Menu2::ALIGN_TOPLEFT);
  }

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const {
    return controller_->IsCommandCheckedForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_index_);
  }
  virtual bool IsCommandIdEnabled(int command_id) const {
    return controller_->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_index_);
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator) {
    return controller_->tabstrip_->GetWidget()->GetAccelerator(command_id,
                                                               accelerator);
  }
  virtual void ExecuteCommand(int command_id) {
    controller_->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_index_);
  }

 private:
  void Build() {
    menu_.reset(new views::Menu2(&model_));
  }

  TabMenuModel model_;
  scoped_ptr<views::Menu2> menu_;

  // The index of the tab we are showing the context menu for.
  int tab_index_;

  // A pointer back to our hosting controller, for command state information.
  BrowserTabStripController* controller_;

  DISALLOW_COPY_AND_ASSIGN(TabContextMenuContents);
};


////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, public:

BrowserTabStripController::BrowserTabStripController(TabStripModel* model,
                                                     SideTabStrip* tabstrip)
    : model_(model),
      tabstrip_(tabstrip) {
  model_->AddObserver(this);
}

BrowserTabStripController::~BrowserTabStripController() {
  model_->RemoveObserver(this);
}

void BrowserTabStripController::InitFromModel() {
  // Walk the model, calling our insertion observer method for each item within
  // it.
  for (int i = 0; i < model_->count(); ++i) {
    TabInsertedAt(model_->GetTabContentsAt(i), i,
                  i == model_->selected_index());
  }
}

bool BrowserTabStripController::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id, int tab_index) const {
  if (model_->ContainsIndex(tab_index))
    return model_->IsContextMenuCommandEnabled(tab_index, command_id);
  return false;
}

bool BrowserTabStripController::IsCommandCheckedForTab(
    TabStripModel::ContextMenuCommand command_id, int tab_index) const {
  // TODO(beng): move to TabStripModel, see note in IsTabPinned.
  if (command_id == TabStripModel::CommandTogglePinned)
    return false;

  if (model_->ContainsIndex(tab_index))
    return model_->IsContextMenuCommandChecked(tab_index, command_id);
  return false;
}

void BrowserTabStripController::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id, int tab_index) {
  if (model_->ContainsIndex(tab_index))
    model_->ExecuteContextMenuCommand(tab_index, command_id);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, SideTabStripModel implementation:

SkBitmap BrowserTabStripController::GetIcon(int index) const {
  return model_->GetTabContentsAt(index)->GetFavIcon();
}

string16 BrowserTabStripController::GetTitle(int index) const {
  return model_->GetTabContentsAt(index)->GetTitle();
}

bool BrowserTabStripController::IsSelected(int index) const {
  return model_->selected_index() == index;
}

SideTabStripModel::NetworkState
    BrowserTabStripController::GetNetworkState(int index) const {
  TabContents* contents = model_->GetTabContentsAt(index);
  if (!contents || !contents->is_loading())
    return NetworkState_None;
  if (contents->waiting_for_response())
    return NetworkState_Waiting;
  return NetworkState_Loading;
}

void BrowserTabStripController::SelectTab(int index) {
  model_->SelectTabContentsAt(index, true);
}

void BrowserTabStripController::CloseTab(int index) {
  model_->CloseTabContentsAt(index);
}

void BrowserTabStripController::ShowContextMenu(int index,
                                                const gfx::Point& p) {
  if (!context_menu_contents_.get())
    context_menu_contents_.reset(new TabContextMenuContents(index, this));
  context_menu_contents_->RunMenuAt(p);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::TabInsertedAt(TabContents* contents, int index,
                                              bool foreground) {
  tabstrip_->AddTabAt(index);
}

void BrowserTabStripController::TabDetachedAt(TabContents* contents,
                                              int index) {
  tabstrip_->RemoveTabAt(index);
}

void BrowserTabStripController::TabSelectedAt(TabContents* old_contents,
                                              TabContents* contents, int index,
                                              bool user_gesture) {
  tabstrip_->SelectTabAt(index);
}

void BrowserTabStripController::TabMoved(TabContents* contents, int from_index,
                                         int to_index) {
}

void BrowserTabStripController::TabChangedAt(TabContents* contents, int index,
                                             TabChangeType change_type) {
  tabstrip_->UpdateTabAt(index);
}

void BrowserTabStripController::TabReplacedAt(TabContents* old_contents,
                                              TabContents* new_contents,
                                              int index) {
}

void BrowserTabStripController::TabPinnedStateChanged(TabContents* contents,
                                                      int index) {
}

void BrowserTabStripController::TabBlockedStateChanged(TabContents* contents,
                                                       int index) {
}

