// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
// TODO(alicet): clean up dependencies on defaults.h and max tab count.
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tabs/tab_strip_selection_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/views/tabs/base_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_service.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

static TabRendererData::NetworkState TabContentsNetworkState(
    TabContents* contents) {
  if (!contents || !contents->IsLoading())
    return TabRendererData::NETWORK_STATE_NONE;
  if (contents->waiting_for_response())
    return TabRendererData::NETWORK_STATE_WAITING;
  return TabRendererData::NETWORK_STATE_LOADING;
}

class BrowserTabStripController::TabContextMenuContents
    : public ui::SimpleMenuModel::Delegate {
 public:
  TabContextMenuContents(BaseTab* tab,
                         BrowserTabStripController* controller)
      : tab_(tab),
        controller_(controller),
        last_command_(TabStripModel::CommandFirst) {
    model_.reset(new TabMenuModel(
        this, controller->model_,
        controller->tabstrip_->GetModelIndexOfBaseTab(tab)));
    menu_model_adapter_.reset(new views::MenuModelAdapter(model_.get()));
    menu_runner_.reset(
        new views::MenuRunner(menu_model_adapter_->CreateMenu()));
  }

  virtual ~TabContextMenuContents() {
    if (controller_)
      controller_->tabstrip_->StopAllHighlighting();
  }

  void Cancel() {
    controller_ = NULL;
  }

  void RunMenuAt(const gfx::Point& point) {
    if (menu_runner_->RunMenuAt(
            tab_->GetWidget(), NULL, gfx::Rect(point, gfx::Size()),
            views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS) ==
        views::MenuRunner::MENU_DELETED)
      return;
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return controller_->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    int browser_cmd;
    return TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                             &browser_cmd) ?
        controller_->tabstrip_->GetWidget()->GetAccelerator(browser_cmd,
                                                            accelerator) :
        false;
  }
  virtual void CommandIdHighlighted(int command_id) OVERRIDE {
    controller_->StopHighlightTabsForCommand(last_command_, tab_);
    last_command_ = static_cast<TabStripModel::ContextMenuCommand>(command_id);
    controller_->StartHighlightTabsForCommand(last_command_, tab_);
  }
  virtual void ExecuteCommand(int command_id) OVERRIDE {
    // Executing the command destroys |this|, and can also end up destroying
    // |controller_|. So stop the highlights before executing the command.
    controller_->tabstrip_->StopAllHighlighting();
    controller_->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }

  virtual void MenuClosed(ui::SimpleMenuModel* /*source*/) OVERRIDE {
    if (controller_)
      controller_->tabstrip_->StopAllHighlighting();
  }

 private:
  scoped_ptr<TabMenuModel> model_;
  scoped_ptr<views::MenuModelAdapter> menu_model_adapter_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The tab we're showing a menu for.
  BaseTab* tab_;

  // A pointer back to our hosting controller, for command state information.
  BrowserTabStripController* controller_;

  // The last command that was selected, so that we can start/stop highlighting
  // appropriately as the user moves through the menu.
  TabStripModel::ContextMenuCommand last_command_;

  DISALLOW_COPY_AND_ASSIGN(TabContextMenuContents);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, public:

BrowserTabStripController::BrowserTabStripController(Browser* browser,
                                                     TabStripModel* model)
    : model_(model),
      tabstrip_(NULL),
      browser_(browser),
      hover_tab_selector_(model) {
  model_->AddObserver(this);

  notification_registrar_.Add(this,
      chrome::NOTIFICATION_TAB_CLOSEABLE_STATE_CHANGED,
      content::NotificationService::AllSources());
}

BrowserTabStripController::~BrowserTabStripController() {
  // When we get here the TabStrip is being deleted. We need to explicitly
  // cancel the menu, otherwise it may try to invoke something on the tabstrip
  // from it's destructor.
  if (context_menu_contents_.get())
    context_menu_contents_->Cancel();

  model_->RemoveObserver(this);
}

void BrowserTabStripController::InitFromModel(BaseTabStrip* tabstrip) {
  tabstrip_ = tabstrip;
  // Walk the model, calling our insertion observer method for each item within
  // it.
  for (int i = 0; i < model_->count(); ++i)
    TabInsertedAt(model_->GetTabContentsAt(i), i, model_->active_index() == i);
}

bool BrowserTabStripController::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id,
    BaseTab* tab) const {
  int model_index = tabstrip_->GetModelIndexOfBaseTab(tab);
  return model_->ContainsIndex(model_index) ?
      model_->IsContextMenuCommandEnabled(model_index, command_id) : false;
}

void BrowserTabStripController::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id,
    BaseTab* tab) {
  int model_index = tabstrip_->GetModelIndexOfBaseTab(tab);
  if (model_->ContainsIndex(model_index))
    model_->ExecuteContextMenuCommand(model_index, command_id);
}

bool BrowserTabStripController::IsTabPinned(BaseTab* tab) const {
  return IsTabPinned(tabstrip_->GetModelIndexOfBaseTab(tab));
}

const TabStripSelectionModel& BrowserTabStripController::GetSelectionModel() {
  return model_->selection_model();
}

int BrowserTabStripController::GetCount() const {
  return model_->count();
}

bool BrowserTabStripController::IsValidIndex(int index) const {
  return model_->ContainsIndex(index);
}

bool BrowserTabStripController::IsActiveTab(int model_index) const {
  return model_->active_index() == model_index;
}

bool BrowserTabStripController::IsTabSelected(int model_index) const {
  return model_->IsTabSelected(model_index);
}

bool BrowserTabStripController::IsTabPinned(int model_index) const {
  return model_->ContainsIndex(model_index) && model_->IsTabPinned(model_index);
}

bool BrowserTabStripController::IsTabCloseable(int model_index) const {
  return !model_->ContainsIndex(model_index) ||
      model_->delegate()->CanCloseTab();
}

bool BrowserTabStripController::IsNewTabPage(int model_index) const {
  return model_->ContainsIndex(model_index) &&
      model_->GetTabContentsAt(model_index)->tab_contents()->GetURL() ==
      GURL(chrome::kChromeUINewTabURL);
}

void BrowserTabStripController::SelectTab(int model_index) {
  model_->ActivateTabAt(model_index, true);
}

void BrowserTabStripController::ExtendSelectionTo(int model_index) {
  model_->ExtendSelectionTo(model_index);
}

void BrowserTabStripController::ToggleSelected(int model_index) {
  model_->ToggleSelectionAt(model_index);
}

void BrowserTabStripController::AddSelectionFromAnchorTo(int model_index) {
  model_->AddSelectionFromAnchorTo(model_index);
}

void BrowserTabStripController::CloseTab(int model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->PrepareForCloseAt(model_index);
  model_->CloseTabContentsAt(model_index,
                             TabStripModel::CLOSE_USER_GESTURE |
                             TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void BrowserTabStripController::ShowContextMenuForTab(BaseTab* tab,
                                                      const gfx::Point& p) {
  context_menu_contents_.reset(new TabContextMenuContents(tab, this));
  context_menu_contents_->RunMenuAt(p);
}

void BrowserTabStripController::UpdateLoadingAnimations() {
  // Don't use the model count here as it's possible for this to be invoked
  // before we've applied an update from the model (Browser::TabInsertedAt may
  // be processed before us and invokes this).
  for (int tab_index = 0, tab_count = tabstrip_->tab_count();
       tab_index < tab_count; ++tab_index) {
    BaseTab* tab = tabstrip_->base_tab_at_tab_index(tab_index);
    int model_index = tabstrip_->GetModelIndexOfBaseTab(tab);
    if (model_->ContainsIndex(model_index)) {
      TabContentsWrapper* contents = model_->GetTabContentsAt(model_index);
      tab->UpdateLoadingAnimation(
          TabContentsNetworkState(contents->tab_contents()));
    }
  }
}

int BrowserTabStripController::HasAvailableDragActions() const {
  return model_->delegate()->GetDragActions();
}

void BrowserTabStripController::OnDropIndexUpdate(int index,
                                                  bool drop_before) {
  // Perform a delayed tab transition if hovering directly over a tab.
  // Otherwise, cancel the pending one.
  if (index != -1 && !drop_before) {
    hover_tab_selector_.StartTabTransition(index);
  } else {
    hover_tab_selector_.CancelTabTransition();
  }
}

void BrowserTabStripController::PerformDrop(bool drop_before,
                                            int index,
                                            const GURL& url) {
  browser::NavigateParams params(
      browser_, url, content::PAGE_TRANSITION_LINK);
  params.tabstrip_index = index;

  if (drop_before) {
    UserMetrics::RecordAction(UserMetricsAction("Tab_DropURLBetweenTabs"));
    params.disposition = NEW_FOREGROUND_TAB;
  } else {
    UserMetrics::RecordAction(UserMetricsAction("Tab_DropURLOnTab"));
    params.disposition = CURRENT_TAB;
    params.source_contents = model_->GetTabContentsAt(index);
  }
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  browser::Navigate(&params);
}

bool BrowserTabStripController::IsCompatibleWith(BaseTabStrip* other) const {
  Profile* other_profile =
      static_cast<BrowserTabStripController*>(other->controller())->profile();
  return other_profile == profile();
}

void BrowserTabStripController::CreateNewTab() {
  UserMetrics::RecordAction(UserMetricsAction("NewTab_Button"));
  model_->delegate()->AddBlankTab(true);
}

void BrowserTabStripController::ClickActiveTab(int index) {
  DCHECK(model_->active_index() == index);
  model_->ActiveTabClicked(index);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::TabInsertedAt(TabContentsWrapper* contents,
                                              int model_index,
                                              bool active) {
  DCHECK(contents);
  DCHECK(model_index == TabStripModel::kNoTab ||
         model_->ContainsIndex(model_index));

  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  TabRendererData data;
  SetTabRendererDataFromModel(contents->tab_contents(), model_index, &data,
                              NEW_TAB);
  tabstrip_->AddTabAt(model_index, data);
}

void BrowserTabStripController::TabDetachedAt(TabContentsWrapper* contents,
                                              int model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->RemoveTabAt(model_index);
}

void BrowserTabStripController::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const TabStripSelectionModel& old_model) {
  tabstrip_->SetSelection(old_model, model_->selection_model());
}

void BrowserTabStripController::TabMoved(TabContentsWrapper* contents,
                                         int from_model_index,
                                         int to_model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  // Update the data first as the pinned state may have changed.
  TabRendererData data;
  SetTabRendererDataFromModel(contents->tab_contents(), to_model_index, &data,
                              EXISTING_TAB);
  tabstrip_->SetTabData(from_model_index, data);

  tabstrip_->MoveTab(from_model_index, to_model_index);
}

void BrowserTabStripController::TabChangedAt(TabContentsWrapper* contents,
                                             int model_index,
                                             TabChangeType change_type) {
  if (change_type == TITLE_NOT_LOADING) {
    tabstrip_->TabTitleChangedNotLoading(model_index);
    // We'll receive another notification of the change asynchronously.
    return;
  }

  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabReplacedAt(TabStripModel* tab_strip_model,
                                              TabContentsWrapper* old_contents,
                                              TabContentsWrapper* new_contents,
                                              int model_index) {
  SetTabDataAt(new_contents, model_index);
}

void BrowserTabStripController::TabPinnedStateChanged(
    TabContentsWrapper* contents,
    int model_index) {
  // Currently none of the renderers render pinned state differently.
}

void BrowserTabStripController::TabMiniStateChanged(
    TabContentsWrapper* contents,
    int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabBlockedStateChanged(
    TabContentsWrapper* contents,
    int model_index) {
  SetTabDataAt(contents, model_index);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, content::NotificationObserver implementation:

void BrowserTabStripController::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CLOSEABLE_STATE_CHANGED);
  // Note that this notification may be fired during a model mutation and
  // possibly before the tabstrip has processed the change.
  // Here, we just re-layout each existing tab to reflect the change in its
  // closeable state, and then schedule paint for entire tabstrip.
  for (int i = 0; i < tabstrip_->tab_count(); ++i)
    tabstrip_->base_tab_at_tab_index(i)->Layout();
  tabstrip_->SchedulePaint();
}

void BrowserTabStripController::SetTabRendererDataFromModel(
    TabContents* contents,
    int model_index,
    TabRendererData* data,
    TabStatus tab_status) {
  SkBitmap* app_icon = NULL;
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(contents);

  // Extension App icons are slightly larger than favicons, so only allow
  // them if permitted by the model.
  if (model_->delegate()->LargeIconsPermitted())
    app_icon = wrapper->extension_tab_helper()->GetExtensionAppIcon();

  if (app_icon)
    data->favicon = *app_icon;
  else
    data->favicon = wrapper->favicon_tab_helper()->GetFavicon();
  data->network_state = TabContentsNetworkState(contents);
  data->title = contents->GetTitle();
  data->url = contents->GetURL();
  data->loading = contents->IsLoading();
  data->crashed_status = contents->crashed_status();
  data->incognito = contents->browser_context()->IsOffTheRecord();
  data->show_icon = wrapper->favicon_tab_helper()->ShouldDisplayFavicon();
  data->mini = model_->IsMiniTab(model_index);
  data->blocked = model_->IsTabBlocked(model_index);
  data->app = wrapper->extension_tab_helper()->is_app();
}

void BrowserTabStripController::SetTabDataAt(
    TabContentsWrapper* contents,
    int model_index) {
  TabRendererData data;
  SetTabRendererDataFromModel(contents->tab_contents(), model_index, &data,
                              EXISTING_TAB);
  tabstrip_->SetTabData(model_index, data);
}

void BrowserTabStripController::StartHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    BaseTab* tab) {
  if (command_id == TabStripModel::CommandCloseOtherTabs ||
      command_id == TabStripModel::CommandCloseTabsToRight) {
    int model_index = tabstrip_->GetModelIndexOfBaseTab(tab);
    if (IsValidIndex(model_index)) {
      std::vector<int> indices =
          model_->GetIndicesClosedByCommand(model_index, command_id);
      for (std::vector<int>::const_iterator i = indices.begin();
           i != indices.end(); ++i) {
        tabstrip_->StartHighlight(*i);
      }
    }
  }
}

void BrowserTabStripController::StopHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    BaseTab* tab) {
  if (command_id == TabStripModel::CommandCloseTabsToRight ||
      command_id == TabStripModel::CommandCloseOtherTabs) {
    // Just tell all Tabs to stop pulsing - it's safe.
    tabstrip_->StopAllHighlighting();
  }
}
