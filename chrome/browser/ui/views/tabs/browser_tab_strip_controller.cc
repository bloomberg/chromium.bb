// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/layout.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using content::UserMetricsAction;
using content::WebContents;

namespace {

TabRendererData::NetworkState TabContentsNetworkState(
    WebContents* contents) {
  if (!contents || !contents->IsLoading())
    return TabRendererData::NETWORK_STATE_NONE;
  if (contents->IsWaitingForResponse())
    return TabRendererData::NETWORK_STATE_WAITING;
  return TabRendererData::NETWORK_STATE_LOADING;
}

TabStripLayoutType DetermineTabStripLayout(PrefService* prefs,
                                           bool* adjust_layout) {
  *adjust_layout = false;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStackedTabStrip)) {
    return TAB_STRIP_LAYOUT_STACKED;
  }
  // For chromeos always allow entering stacked mode.
#if !defined(OS_CHROMEOS)
  if (ui::GetDisplayLayout() != ui::LAYOUT_TOUCH)
    return TAB_STRIP_LAYOUT_SHRINK;
#endif
  *adjust_layout = true;
  switch (prefs->GetInteger(prefs::kTabStripLayoutType)) {
    case TAB_STRIP_LAYOUT_STACKED:
      return TAB_STRIP_LAYOUT_STACKED;
    default:
      return TAB_STRIP_LAYOUT_SHRINK;
  }
}

}  // namespace

class BrowserTabStripController::TabContextMenuContents
    : public ui::SimpleMenuModel::Delegate {
 public:
  TabContextMenuContents(Tab* tab,
                         BrowserTabStripController* controller)
      : tab_(tab),
        controller_(controller),
        last_command_(TabStripModel::CommandFirst) {
    model_.reset(new TabMenuModel(
        this, controller->model_,
        controller->tabstrip_->GetModelIndexOfTab(tab)));
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
            views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS |
            views::MenuRunner::CONTEXT_MENU) ==
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
  Tab* tab_;

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

  local_pref_registrar_.Init(g_browser_process->local_state());
  local_pref_registrar_.Add(
      prefs::kTabStripLayoutType,
      base::Bind(&BrowserTabStripController::UpdateLayoutType,
                 base::Unretained(this)));
}

BrowserTabStripController::~BrowserTabStripController() {
  // When we get here the TabStrip is being deleted. We need to explicitly
  // cancel the menu, otherwise it may try to invoke something on the tabstrip
  // from its destructor.
  if (context_menu_contents_.get())
    context_menu_contents_->Cancel();

  model_->RemoveObserver(this);
}

void BrowserTabStripController::InitFromModel(TabStrip* tabstrip) {
  tabstrip_ = tabstrip;

  UpdateLayoutType();

  // Walk the model, calling our insertion observer method for each item within
  // it.
  for (int i = 0; i < model_->count(); ++i)
    AddTab(model_->GetWebContentsAt(i), i, model_->active_index() == i);
}

bool BrowserTabStripController::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) const {
  int model_index = tabstrip_->GetModelIndexOfTab(tab);
  return model_->ContainsIndex(model_index) ?
      model_->IsContextMenuCommandEnabled(model_index, command_id) : false;
}

void BrowserTabStripController::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  int model_index = tabstrip_->GetModelIndexOfTab(tab);
  if (model_->ContainsIndex(model_index))
    model_->ExecuteContextMenuCommand(model_index, command_id);
}

bool BrowserTabStripController::IsTabPinned(Tab* tab) const {
  return IsTabPinned(tabstrip_->GetModelIndexOfTab(tab));
}

const ui::ListSelectionModel& BrowserTabStripController::GetSelectionModel() {
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

int BrowserTabStripController::GetActiveIndex() const {
  return model_->active_index();
}

bool BrowserTabStripController::IsTabSelected(int model_index) const {
  return model_->IsTabSelected(model_index);
}

bool BrowserTabStripController::IsTabPinned(int model_index) const {
  return model_->ContainsIndex(model_index) && model_->IsTabPinned(model_index);
}

bool BrowserTabStripController::IsNewTabPage(int model_index) const {
  return model_->ContainsIndex(model_index) &&
      model_->GetWebContentsAt(model_index)->GetURL() ==
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

void BrowserTabStripController::CloseTab(int model_index,
                                         CloseTabSource source) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->PrepareForCloseAt(model_index, source);
  model_->CloseWebContentsAt(model_index,
                             TabStripModel::CLOSE_USER_GESTURE |
                             TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void BrowserTabStripController::ShowContextMenuForTab(Tab* tab,
                                                      const gfx::Point& p) {
  context_menu_contents_.reset(new TabContextMenuContents(tab, this));
  context_menu_contents_->RunMenuAt(p);
}

void BrowserTabStripController::UpdateLoadingAnimations() {
  // Don't use the model count here as it's possible for this to be invoked
  // before we've applied an update from the model (Browser::TabInsertedAt may
  // be processed before us and invokes this).
  for (int i = 0, tab_count = tabstrip_->tab_count(); i < tab_count; ++i) {
    if (model_->ContainsIndex(i)) {
      Tab* tab = tabstrip_->tab_at(i);
      WebContents* contents = model_->GetWebContentsAt(i);
      tab->UpdateLoadingAnimation(TabContentsNetworkState(contents));
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
  chrome::NavigateParams params(browser_, url, content::PAGE_TRANSITION_LINK);
  params.tabstrip_index = index;

  if (drop_before) {
    content::RecordAction(UserMetricsAction("Tab_DropURLBetweenTabs"));
    params.disposition = NEW_FOREGROUND_TAB;
  } else {
    content::RecordAction(UserMetricsAction("Tab_DropURLOnTab"));
    params.disposition = CURRENT_TAB;
    params.source_contents = model_->GetWebContentsAt(index);
  }
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

bool BrowserTabStripController::IsCompatibleWith(TabStrip* other) const {
  Profile* other_profile =
      static_cast<BrowserTabStripController*>(other->controller())->profile();
  return other_profile == profile();
}

void BrowserTabStripController::CreateNewTab() {
  model_->delegate()->AddBlankTabAt(-1, true);
}

bool BrowserTabStripController::IsIncognito() {
  return browser_->profile()->IsOffTheRecord();
}

void BrowserTabStripController::LayoutTypeMaybeChanged() {
  bool adjust_layout = false;
  TabStripLayoutType layout_type =
      DetermineTabStripLayout(g_browser_process->local_state(), &adjust_layout);
  if (!adjust_layout || layout_type == tabstrip_->layout_type())
    return;

  g_browser_process->local_state()->SetInteger(
      prefs::kTabStripLayoutType,
      static_cast<int>(tabstrip_->layout_type()));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::TabInsertedAt(WebContents* contents,
                                              int model_index,
                                              bool is_active) {
  DCHECK(contents);
  DCHECK(model_->ContainsIndex(model_index));
  AddTab(contents, model_index, is_active);
}

void BrowserTabStripController::TabDetachedAt(WebContents* contents,
                                              int model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->RemoveTabAt(model_index);
}

void BrowserTabStripController::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  tabstrip_->SetSelection(old_model, model_->selection_model());
}

void BrowserTabStripController::TabMoved(WebContents* contents,
                                         int from_model_index,
                                         int to_model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  // Pass in the TabRendererData as the pinned state may have changed.
  TabRendererData data;
  SetTabRendererDataFromModel(contents, to_model_index, &data, EXISTING_TAB);
  tabstrip_->MoveTab(from_model_index, to_model_index, data);
}

void BrowserTabStripController::TabChangedAt(WebContents* contents,
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
                                              WebContents* old_contents,
                                              WebContents* new_contents,
                                              int model_index) {
  SetTabDataAt(new_contents, model_index);
}

void BrowserTabStripController::TabPinnedStateChanged(WebContents* contents,
                                                      int model_index) {
  // Currently none of the renderers render pinned state differently.
}

void BrowserTabStripController::TabMiniStateChanged(WebContents* contents,
                                                    int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabBlockedStateChanged(WebContents* contents,
                                                       int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::SetTabRendererDataFromModel(
    WebContents* contents,
    int model_index,
    TabRendererData* data,
    TabStatus tab_status) {
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(contents);

  data->favicon = favicon_tab_helper->GetFavicon().AsImageSkia();
  data->network_state = TabContentsNetworkState(contents);
  data->title = contents->GetTitle();
  data->url = contents->GetURL();
  data->loading = contents->IsLoading();
  data->crashed_status = contents->GetCrashedStatus();
  data->incognito = contents->GetBrowserContext()->IsOffTheRecord();
  data->show_icon = favicon_tab_helper->ShouldDisplayFavicon();
  data->mini = model_->IsMiniTab(model_index);
  data->blocked = model_->IsTabBlocked(model_index);
  data->app = extensions::TabHelper::FromWebContents(contents)->is_app();
  if (chrome::ShouldShowProjectingIndicator(contents))
    data->capture_state = TabRendererData::CAPTURE_STATE_PROJECTING;
  else if (chrome::ShouldShowRecordingIndicator(contents))
    data->capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  else
    data->capture_state = TabRendererData::CAPTURE_STATE_NONE;

  if (chrome::ShouldShowAudioIndicator(contents))
    data->audio_state = TabRendererData::AUDIO_STATE_PLAYING;
  else
    data->audio_state = TabRendererData::AUDIO_STATE_NONE;
}

void BrowserTabStripController::SetTabDataAt(content::WebContents* web_contents,
                                             int model_index) {
  TabRendererData data;
  SetTabRendererDataFromModel(web_contents, model_index, &data, EXISTING_TAB);
  tabstrip_->SetTabData(model_index, data);
}

void BrowserTabStripController::StartHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  if (command_id == TabStripModel::CommandCloseOtherTabs ||
      command_id == TabStripModel::CommandCloseTabsToRight) {
    int model_index = tabstrip_->GetModelIndexOfTab(tab);
    if (IsValidIndex(model_index)) {
      std::vector<int> indices =
          model_->GetIndicesClosedByCommand(model_index, command_id);
      for (std::vector<int>::const_iterator i(indices.begin());
           i != indices.end(); ++i) {
        tabstrip_->StartHighlight(*i);
      }
    }
  }
}

void BrowserTabStripController::StopHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  if (command_id == TabStripModel::CommandCloseTabsToRight ||
      command_id == TabStripModel::CommandCloseOtherTabs) {
    // Just tell all Tabs to stop pulsing - it's safe.
    tabstrip_->StopAllHighlighting();
  }
}

void BrowserTabStripController::AddTab(WebContents* contents,
                                       int index,
                                       bool is_active) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  TabRendererData data;
  SetTabRendererDataFromModel(contents, index, &data, NEW_TAB);
  tabstrip_->AddTabAt(index, data, is_active);
}

void BrowserTabStripController::UpdateLayoutType() {
  bool adjust_layout = false;
  TabStripLayoutType layout_type =
      DetermineTabStripLayout(g_browser_process->local_state(), &adjust_layout);
  tabstrip_->SetLayoutType(layout_type, adjust_layout);
}
