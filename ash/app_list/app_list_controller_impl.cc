// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_observer.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_delegate_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_switches.h"
#include "extensions/common/constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

bool IsTabletMode() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

// Close current Assistant UI.
void CloseAssistantUi(AssistantExitPoint exit_point) {
  if (app_list_features::IsEmbeddedAssistantUIEnabled())
    Shell::Get()->assistant_controller()->ui_controller()->CloseUi(exit_point);
}

// Minimize all windows that aren't the app list in reverse order to preserve
// the mru ordering.
// Returns false if no window is minimized.
bool MinimizeAllWindows() {
  bool handled = false;
  aura::Window* app_list_container =
      Shell::Get()->GetPrimaryRootWindow()->GetChildById(
          kShellWindowId_AppListTabletModeContainer);
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  std::reverse(windows.begin(), windows.end());
  for (auto* window : windows) {
    if (!app_list_container->Contains(window) &&
        !wm::GetWindowState(window)->IsMinimized()) {
      wm::GetWindowState(window)->Minimize();
      handled = true;
    }
  }
  return handled;
}

}  // namespace

AppListControllerImpl::AppListControllerImpl()
    : model_(std::make_unique<app_list::AppListModel>()),
      presenter_(std::make_unique<AppListPresenterDelegateImpl>(this)) {
  model_->AddObserver(this);

  SessionController* session_controller = Shell::Get()->session_controller();
  session_controller->AddObserver(this);

  // In case of crash-and-restart case where session state starts with ACTIVE
  // and does not change to trigger OnSessionStateChanged(), notify the current
  // session state here to ensure that the app list is shown.
  OnSessionStateChanged(session_controller->GetSessionState());

  Shell* shell = Shell::Get();
  shell->tablet_mode_controller()->AddObserver(this);
  shell->wallpaper_controller()->AddObserver(this);
  shell->AddShellObserver(this);
  shell->overview_controller()->AddObserver(this);
  keyboard::KeyboardController::Get()->AddObserver(this);
  shell->voice_interaction_controller()->AddLocalObserver(this);
  shell->window_tree_host_manager()->AddObserver(this);
  shell->mru_window_tracker()->AddObserver(this);
  if (app_list_features::IsEmbeddedAssistantUIEnabled())
    shell->assistant_controller()->ui_controller()->AddModelObserver(this);
  shell->home_screen_controller()->home_launcher_gesture_handler()->AddObserver(
      this);
}

AppListControllerImpl::~AppListControllerImpl() = default;

void AppListControllerImpl::SetClient(mojom::AppListClientPtr client_ptr) {
  client_ = std::move(client_ptr);
}

void AppListControllerImpl::BindRequest(
    mojom::AppListControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

app_list::AppListModel* AppListControllerImpl::GetModel() {
  return model_.get();
}

app_list::SearchModel* AppListControllerImpl::GetSearchModel() {
  return &search_model_;
}

void AppListControllerImpl::AddItem(AppListItemMetadataPtr item_data) {
  const std::string folder_id = item_data->folder_id;
  if (folder_id.empty())
    model_->AddItem(CreateAppListItem(std::move(item_data)));
  else
    AddItemToFolder(std::move(item_data), folder_id);
}

void AppListControllerImpl::AddItemToFolder(AppListItemMetadataPtr item_data,
                                            const std::string& folder_id) {
  // When we're setting a whole model of a profile, each item may have its
  // folder id set properly. However, |AppListModel::AddItemToFolder| requires
  // the item to add is not in the target folder yet, and sets its folder id
  // later. So we should clear the folder id here to avoid breaking checks.
  item_data->folder_id.clear();
  model_->AddItemToFolder(CreateAppListItem(std::move(item_data)), folder_id);
}

void AppListControllerImpl::RemoveItem(const std::string& id) {
  model_->DeleteItem(id);
}

void AppListControllerImpl::RemoveUninstalledItem(const std::string& id) {
  model_->DeleteUninstalledItem(id);
}

void AppListControllerImpl::MoveItemToFolder(const std::string& id,
                                             const std::string& folder_id) {
  app_list::AppListItem* item = model_->FindItem(id);
  model_->MoveItemToFolder(item, folder_id);
}

void AppListControllerImpl::SetStatus(ash::AppListModelStatus status) {
  model_->SetStatus(status);
}

void AppListControllerImpl::SetState(ash::AppListState state) {
  model_->SetState(state);
}

void AppListControllerImpl::HighlightItemInstalledFromUI(
    const std::string& id) {
  model_->top_level_item_list()->HighlightItemInstalledFromUI(id);
}

void AppListControllerImpl::SetSearchEngineIsGoogle(bool is_google) {
  search_model_.SetSearchEngineIsGoogle(is_google);
}

void AppListControllerImpl::SetSearchTabletAndClamshellAccessibleName(
    const base::string16& tablet_accessible_name,
    const base::string16& clamshell_accessible_name) {
  search_model_.search_box()->SetTabletAndClamshellAccessibleName(
      tablet_accessible_name, clamshell_accessible_name);
}

void AppListControllerImpl::SetSearchHintText(const base::string16& hint_text) {
  search_model_.search_box()->SetHintText(hint_text);
}

void AppListControllerImpl::UpdateSearchBox(const base::string16& text,
                                            bool initiated_by_user) {
  search_model_.search_box()->Update(text, initiated_by_user);
}

void AppListControllerImpl::PublishSearchResults(
    std::vector<SearchResultMetadataPtr> results) {
  std::vector<std::unique_ptr<app_list::SearchResult>> new_results;
  for (auto& result_metadata : results) {
    std::unique_ptr<app_list::SearchResult> result =
        std::make_unique<app_list::SearchResult>();
    result->SetMetadata(std::move(result_metadata));
    new_results.push_back(std::move(result));
  }
  search_model_.PublishResults(std::move(new_results));
}

void AppListControllerImpl::SetItemMetadata(const std::string& id,
                                            AppListItemMetadataPtr data) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (!item)
    return;

  // data may not contain valid position or icon. Preserve it in this case.
  if (!data->position.IsValid())
    data->position = item->position();

  // Update the item's position and name based on the metadata.
  if (!data->position.Equals(item->position()))
    model_->SetItemPosition(item, data->position);

  if (data->short_name.empty()) {
    if (data->name != item->name()) {
      model_->SetItemName(item, data->name);
    }
  } else {
    if (data->name != item->name() || data->short_name != item->short_name()) {
      model_->SetItemNameAndShortName(item, data->name, data->short_name);
    }
  }

  // Folder icon is generated on ash side and chrome side passes a null
  // icon here. Skip it.
  if (data->icon.isNull())
    data->icon = item->icon();
  item->SetMetadata(std::move(data));
}

void AppListControllerImpl::SetItemIcon(const std::string& id,
                                        const gfx::ImageSkia& icon) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIcon(icon);
}

void AppListControllerImpl::SetItemIsInstalling(const std::string& id,
                                                bool is_installing) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIsInstalling(is_installing);
}

void AppListControllerImpl::SetItemPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetPercentDownloaded(percent_downloaded);
}

void AppListControllerImpl::SetModelData(
    std::vector<AppListItemMetadataPtr> apps,
    bool is_search_engine_google) {
  // Clear old model data.
  model_->DeleteAllItems();
  search_model_.DeleteAllResults();

  // Populate new models. First populate folders and then other items to avoid
  // automatically creating folder items in |AddItemToFolder|.
  for (auto& app : apps) {
    if (!app->is_folder)
      continue;
    DCHECK(app->folder_id.empty());
    AddItem(std::move(app));
  }
  for (auto& app : apps) {
    if (!app)
      continue;
    AddItem(std::move(app));
  }
  search_model_.SetSearchEngineIsGoogle(is_search_engine_google);
}

void AppListControllerImpl::SetSearchResultMetadata(
    SearchResultMetadataPtr metadata) {
  app_list::SearchResult* result = search_model_.FindSearchResult(metadata->id);
  if (result)
    result->SetMetadata(std::move(metadata));
}

void AppListControllerImpl::SetSearchResultIsInstalling(const std::string& id,
                                                        bool is_installing) {
  app_list::SearchResult* result = search_model_.FindSearchResult(id);
  if (result)
    result->SetIsInstalling(is_installing);
}

void AppListControllerImpl::SetSearchResultPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  app_list::SearchResult* result = search_model_.FindSearchResult(id);
  if (result)
    result->SetPercentDownloaded(percent_downloaded);
}

void AppListControllerImpl::NotifySearchResultItemInstalled(
    const std::string& id) {
  app_list::SearchResult* result = search_model_.FindSearchResult(id);
  if (result)
    result->NotifyItemInstalled();
}

void AppListControllerImpl::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  base::flat_map<std::string, uint16_t> id_to_app_list_index;
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    id_to_app_list_index[model_->top_level_item_list()->item_at(i)->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

void AppListControllerImpl::FindOrCreateOemFolder(
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position,
    FindOrCreateOemFolderCallback callback) {
  app_list::AppListFolderItem* oem_folder =
      model_->FindFolderItem(kOemFolderId);
  if (!oem_folder) {
    std::unique_ptr<app_list::AppListFolderItem> new_folder =
        std::make_unique<app_list::AppListFolderItem>(kOemFolderId);
    syncer::StringOrdinal oem_position = preferred_oem_position.IsValid()
                                             ? preferred_oem_position
                                             : GetOemFolderPos();
    // Do not create a sync item for the OEM folder here, do it in
    // ResolveFolderPositions() when the item position is finalized.
    oem_folder = static_cast<app_list::AppListFolderItem*>(
        model_->AddItem(std::move(new_folder)));
    model_->SetItemPosition(oem_folder, oem_position);
  }
  model_->SetItemName(oem_folder, oem_folder_name);
  std::move(callback).Run(oem_folder->CloneMetadata());
}

void AppListControllerImpl::ResolveOemFolderPosition(
    const syncer::StringOrdinal& preferred_oem_position,
    ResolveOemFolderPositionCallback callback) {
  // In ash:
  app_list::AppListFolderItem* ash_oem_folder = FindFolderItem(kOemFolderId);
  ash::mojom::AppListItemMetadataPtr metadata = nullptr;
  if (ash_oem_folder) {
    const syncer::StringOrdinal& oem_folder_pos =
        preferred_oem_position.IsValid() ? preferred_oem_position
                                         : GetOemFolderPos();
    model_->SetItemPosition(ash_oem_folder, oem_folder_pos);
    metadata = ash_oem_folder->CloneMetadata();
  }
  std::move(callback).Run(std::move(metadata));
}

void AppListControllerImpl::DismissAppList() {
  presenter_.Dismiss(base::TimeTicks());
}

void AppListControllerImpl::GetAppInfoDialogBounds(
    GetAppInfoDialogBoundsCallback callback) {
  app_list::AppListView* app_list_view = presenter_.GetView();
  gfx::Rect bounds = gfx::Rect();
  if (app_list_view)
    bounds = app_list_view->GetAppInfoDialogBounds();
  std::move(callback).Run(bounds);
}

void AppListControllerImpl::ShowAppListAndSwitchToState(
    ash::AppListState state) {
  bool app_list_was_open = true;
  app_list::AppListView* app_list_view = presenter_.GetView();
  if (!app_list_view) {
    // TODO(calamity): This may cause the app list to show briefly before the
    // state change. If this becomes an issue, add the ability to ash::Shell to
    // load the app list without showing it.
    presenter_.Show(GetDisplayIdToShowAppListOn(), base::TimeTicks());
    app_list_was_open = false;
    app_list_view = presenter_.GetView();
    DCHECK(app_list_view);
  }

  if (state == ash::AppListState::kInvalidState)
    return;

  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();
  contents_view->SetActiveState(state, app_list_was_open /* animate */);
}

void AppListControllerImpl::ShowAppList() {
  presenter_.Show(GetDisplayIdToShowAppListOn(), base::TimeTicks());
}

////////////////////////////////////////////////////////////////////////////////
// app_list::AppListModelObserver:

void AppListControllerImpl::OnAppListItemAdded(app_list::AppListItem* item) {
  if (item->is_folder())
    client_->OnFolderCreated(item->CloneMetadata());
  else if (item->is_page_break())
    client_->OnPageBreakItemAdded(item->id(), item->position());
}

void AppListControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* /* pref_service */) {
  if (!IsTabletMode()) {
    DismissAppList();
    return;
  }

  // Show the app list after signing in in tablet mode.
  Show(GetDisplayIdToShowAppListOn(), app_list::AppListShowSource::kTabletMode,
       base::TimeTicks());

  // The app list is not dismissed before switching user, suggestion chips will
  // not be shown. So reset app list state and trigger an initial search here to
  // update the suggestion results.
  presenter_.GetView()->CloseOpenedPage();
  presenter_.GetView()->search_box_view()->ClearSearch();
}

void AppListControllerImpl::OnAppListItemWillBeDeleted(
    app_list::AppListItem* item) {
  if (!client_)
    return;

  if (item->is_folder())
    client_->OnFolderDeleted(item->CloneMetadata());

  if (item->is_page_break())
    client_->OnPageBreakItemDeleted(item->id());
}

void AppListControllerImpl::OnAppListItemUpdated(app_list::AppListItem* item) {
  if (client_)
    client_->OnItemUpdated(item->CloneMetadata());
}

void AppListControllerImpl::OnAppListStateChanged(ash::AppListState new_state,
                                                  ash::AppListState old_state) {
  if (!app_list_features::IsEmbeddedAssistantUIEnabled())
    return;

  UpdateLauncherContainer();

  if (new_state == ash::AppListState::kStateEmbeddedAssistant) {
    // ShowUi will be no-op if the AssistantUiModel is already visible.
    Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
        ash::AssistantEntryPoint::kUnspecified);
    return;
  }

  if (old_state == ash::AppListState::kStateEmbeddedAssistant) {
    // CloseUi will be no-op if the AssistantUiModel is already closed.
    Shell::Get()->assistant_controller()->ui_controller()->CloseUi(
        ash::AssistantExitPoint::kBackInLauncher);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Methods used in Ash

bool AppListControllerImpl::GetTargetVisibility() const {
  return presenter_.GetTargetVisibility();
}

bool AppListControllerImpl::IsVisible() const {
  return presenter_.IsVisible();
}

void AppListControllerImpl::Show(int64_t display_id,
                                 app_list::AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp) {
  UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                            show_source);
  if (!presenter_.GetTargetVisibility() && IsVisible()) {
    // The launcher is running close animation, so close it immediately before
    // reshow the launcher in tablet mode.
    presenter_.GetView()->GetWidget()->CloseNow();
  }

  presenter_.Show(display_id, event_time_stamp);

  // AppListControllerImpl::Show is called in ash at the first time of showing
  // app list view. So check whether the expand arrow view should be visible.
  UpdateExpandArrowVisibility();
}

void AppListControllerImpl::UpdateYPositionAndOpacity(
    int y_position_in_screen,
    float background_opacity) {
  // Avoid changing app list opacity and position when homecher is enabled.
  if (IsTabletMode())
    return;
  presenter_.UpdateYPositionAndOpacity(y_position_in_screen,
                                       background_opacity);
}

void AppListControllerImpl::EndDragFromShelf(
    app_list::AppListViewState app_list_state) {
  // Avoid dragging app list when homecher is enabled.
  if (IsTabletMode())
    return;
  presenter_.EndDragFromShelf(app_list_state);
}

void AppListControllerImpl::ProcessMouseWheelEvent(
    const ui::MouseWheelEvent& event) {
  presenter_.ProcessMouseWheelOffset(event.offset());
}

ash::ShelfAction AppListControllerImpl::ToggleAppList(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  ash::ShelfAction action =
      presenter_.ToggleAppList(display_id, show_source, event_time_stamp);
  if (action == SHELF_ACTION_APP_LIST_SHOWN) {
    UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                              show_source);
  }
  return action;
}

app_list::AppListViewState AppListControllerImpl::GetAppListViewState() {
  return model_->state_fullscreen();
}

void AppListControllerImpl::FlushForTesting() {
  bindings_.FlushForTesting();
}

// Stop observing at the beginning of ~Shell to avoid unnecessary work during
// Shell shutdown.
void AppListControllerImpl::OnShellDestroying() {
  Shell* shell = Shell::Get();
  shell->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->RemoveObserver(this);
  if (app_list_features::IsEmbeddedAssistantUIEnabled())
    shell->assistant_controller()->ui_controller()->RemoveModelObserver(this);
  shell->mru_window_tracker()->RemoveObserver(this);
  shell->window_tree_host_manager()->RemoveObserver(this);
  shell->voice_interaction_controller()->RemoveLocalObserver(this);
  keyboard::KeyboardController::Get()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->RemoveShellObserver(this);
  shell->wallpaper_controller()->RemoveObserver(this);
  shell->tablet_mode_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  model_->RemoveObserver(this);
}

void AppListControllerImpl::OnOverviewModeStarting() {
  if (!IsTabletMode()) {
    DismissAppList();
    return;
  }

  // Only animate the app list when overview mode is using slide animation.
  presenter_.ScheduleOverviewModeAnimation(
      /*start=*/true,
      Shell::Get()
              ->overview_controller()
              ->overview_session()
              ->enter_exit_overview_type() ==
          OverviewSession::EnterExitOverviewType::kWindowsMinimized);
}

void AppListControllerImpl::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  if (!IsTabletMode())
    return;

  // Animate the launcher if overview mode is sliding out. Let
  // OnOverviewModeEndingAnimationComplete handle showing the launcher after
  // overview mode finishes animating. Overview however is nullptr by the
  // time the animations are finished, so we need to check the animation type
  // here.
  use_slide_to_exit_overview_ =
      overview_session->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kWindowsMinimized;
}

void AppListControllerImpl::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (!IsTabletMode() || canceled)
    return;

  presenter_.ScheduleOverviewModeAnimation(/*start=*/false,
                                           use_slide_to_exit_overview_);
}

void AppListControllerImpl::OnTabletModeStarted() {
  if (presenter_.GetTargetVisibility()) {
    DCHECK(IsVisible());
    presenter_.GetView()->OnTabletModeChanged(true);
  }

  // Show the app list if the tablet mode starts.
  Shell::Get()->home_screen_controller()->Show();
  UpdateLauncherContainer();
}

void AppListControllerImpl::OnTabletModeEnded() {
  if (IsVisible())
    presenter_.GetView()->OnTabletModeChanged(false);

  // Dismiss the app list if the tablet mode ends.
  DismissAppList();
  UpdateLauncherContainer();
}

void AppListControllerImpl::OnWallpaperColorsChanged() {
  if (IsVisible())
    presenter_.GetView()->OnWallpaperColorsChanged();
}

void AppListControllerImpl::OnKeyboardVisibilityStateChanged(
    const bool is_visible) {
  onscreen_keyboard_shown_ = is_visible;
  app_list::AppListView* app_list_view = presenter_.GetView();
  if (app_list_view)
    app_list_view->OnScreenKeyboardShown(is_visible);
}

void AppListControllerImpl::OnVoiceInteractionSettingsEnabled(bool enabled) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnAssistantFeatureAllowedChanged(
    mojom::AssistantAllowedState state) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnDisplayConfigurationChanged() {
  // Entering tablet mode triggers a display configuration change when we
  // automatically switch to mirror mode. Switching to mirror mode happens
  // asynchronously (see DisplayConfigurationObserver::OnTabletModeStarted()).
  // This may result in the removal of a window tree host, as in the example of
  // switching to tablet mode while Unified Desktop mode is on; the Unified host
  // will be destroyed and the Home Launcher (which was created earlier when we
  // entered tablet mode) will be dismissed.
  // To avoid crashes, we must ensure that the Home Launcher shown status is as
  // expected if it's enabled and we're still in tablet mode.
  // https://crbug.com/900956.
  const bool should_be_shown = IsTabletMode();
  if (should_be_shown == GetTargetVisibility())
    return;

  if (should_be_shown)
    Shell::Get()->home_screen_controller()->Show();
}

void AppListControllerImpl::OnWindowUntracked(aura::Window* untracked_window) {
  UpdateExpandArrowVisibility();
}

void AppListControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  switch (new_visibility) {
    case AssistantVisibility::kVisible:
      if (!IsVisible()) {
        Show(GetDisplayIdToShowAppListOn(), app_list::kAssistantEntryPoint,
             base::TimeTicks());
      }

      if (!IsShowingEmbeddedAssistantUI()) {
        if (presenter_.GetView()->app_list_state() ==
            app_list::AppListViewState::PEEKING) {
          presenter_.GetView()->SetState(app_list::AppListViewState::HALF);
        }
        presenter_.ShowEmbeddedAssistantUI(true);
      }
      break;
    case AssistantVisibility::kHidden:
      NOTREACHED();
      break;
    case AssistantVisibility::kClosed:
      if (!IsShowingEmbeddedAssistantUI())
        break;

      // Reset model state.
      presenter_.ShowEmbeddedAssistantUI(false);
      if (IsTabletMode()) {
        presenter_.GetView()->app_list_main_view()->ResetForShow();
        presenter_.GetView()->SetState(
            app_list::AppListViewState::FULLSCREEN_ALL_APPS);
      } else if (exit_point != AssistantExitPoint::kBackInLauncher) {
        DismissAppList();
      }
      break;
  }
}

void AppListControllerImpl::OnHomeLauncherAnimationComplete(
    bool shown,
    int64_t display_id) {
  CloseAssistantUi(shown ? AssistantExitPoint::kLauncherOpen
                         : AssistantExitPoint::kLauncherClose);
}

void AppListControllerImpl::ShowHomeScreen() {
  DCHECK(IsTabletMode());

  Show(GetDisplayIdToShowAppListOn(), app_list::kTabletMode, base::TimeTicks());
}

aura::Window* AppListControllerImpl::GetHomeScreenWindow() {
  return presenter_.GetWindow();
}

void AppListControllerImpl::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    UpdateAnimationSettingsCallback callback) {
  presenter_.UpdateYPositionAndOpacityForHomeLauncher(
      y_position_in_screen, opacity, std::move(callback));
}

void AppListControllerImpl::UpdateAfterHomeLauncherShown() {
  // Show or hide the expand arrow view.
  UpdateExpandArrowVisibility();
}

base::Optional<base::TimeDelta>
AppListControllerImpl::GetOptionalAnimationDuration() {
  if (model_->state() == ash::AppListState::kStateEmbeddedAssistant) {
    // If Assistant is shown, we don't want any delay in animation transitions
    // since the launcher is already shown.
    return base::TimeDelta::Min();
  }
  return base::nullopt;
}

void AppListControllerImpl::Back() {
  presenter_.GetView()->Back();
}

ash::ShelfAction AppListControllerImpl::OnAppListButtonPressed(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  if (!IsTabletMode())
    return ToggleAppList(display_id, show_source, event_time_stamp);

  bool handled =
      Shell::Get()
          ->home_screen_controller()
          ->home_launcher_gesture_handler()
          ->ShowHomeLauncher(
              Shell::Get()->display_manager()->GetDisplayForId(display_id));

  if (!handled) {
    if (Shell::Get()->overview_controller()->IsSelecting()) {
      // End overview mode.
      Shell::Get()->overview_controller()->ToggleOverview(
          OverviewSession::EnterExitOverviewType::kWindowsMinimized);
      handled = true;
    }
    if (Shell::Get()->split_view_controller()->IsSplitViewModeActive()) {
      // End split view mode.
      Shell::Get()->split_view_controller()->EndSplitView(
          SplitViewController::EndReason::kHomeLauncherPressed);
      handled = true;
    }
  }

  if (!handled)
    handled = MinimizeAllWindows();

  // Perform the "back" action for the app list.
  if (!handled)
    Back();

  return ash::SHELF_ACTION_APP_LIST_SHOWN;
}

bool AppListControllerImpl::IsShowingEmbeddedAssistantUI() const {
  return presenter_.IsShowingEmbeddedAssistantUI();
}

void AppListControllerImpl::UpdateExpandArrowVisibility() {
  bool should_show = false;

  // Hide the expand arrow view when tablet mode is enabled and there is no
  // activatable window.
  if (IsTabletMode()) {
    should_show = !ash::Shell::Get()
                       ->mru_window_tracker()
                       ->BuildWindowForCycleList()
                       .empty();
  } else {
    should_show = true;
  }

  presenter_.SetExpandArrowViewVisibility(should_show);
}

app_list::AppListViewState AppListControllerImpl::CalculateStateAfterShelfDrag(
    const ui::GestureEvent& gesture_in_screen,
    float launcher_above_shelf_bottom_amount) const {
  if (presenter_.GetView())
    return presenter_.GetView()->CalculateStateAfterShelfDrag(
        gesture_in_screen, launcher_above_shelf_bottom_amount);
  return app_list::AppListViewState::CLOSED;
}

void AppListControllerImpl::SetAppListModelForTest(
    std::unique_ptr<app_list::AppListModel> model) {
  model_->RemoveObserver(this);
  model_ = std::move(model);
  model_->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Methods of |client_|:

void AppListControllerImpl::StartAssistant() {
  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    ash::Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
        ash::AssistantEntryPoint::kLauncherSearchBoxMic);
    return;
  }

  if (!IsTabletMode())
    DismissAppList();

  ash::Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
      ash::AssistantEntryPoint::kLauncherSearchBox);
}

void AppListControllerImpl::StartSearch(const base::string16& raw_query) {
  last_raw_query_ = raw_query;
  if (client_) {
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    client_->StartSearch(query);
  }
}

void AppListControllerImpl::OpenSearchResult(const std::string& result_id,
                                             int event_flags) {
  app_list::SearchResult* result = search_model_.FindSearchResult(result_id);
  if (!result)
    return;

  UMA_HISTOGRAM_ENUMERATION(app_list::kSearchResultOpenDisplayTypeHistogram,
                            result->display_type(),
                            ash::SearchResultDisplayType::kLast);

  // Record the search metric if the SearchResult is not a suggested app.
  if (result->display_type() != ash::SearchResultDisplayType::kRecommendation) {
    // Count AppList.Search here because it is composed of search + action.
    base::RecordAction(base::UserMetricsAction("AppList_OpenSearchResult"));

    UMA_HISTOGRAM_COUNTS_100(app_list::kSearchQueryLength,
                             last_raw_query_.size());

    if (result->distance_from_origin() >= 0) {
      UMA_HISTOGRAM_COUNTS_100(app_list::kSearchResultDistanceFromOrigin,
                               result->distance_from_origin());
    }
  }

  if (presenter_.IsVisible() && result->is_omnibox_search() &&
      IsAssistantAllowedAndEnabled() &&
      app_list_features::IsEmbeddedAssistantUIEnabled()) {
    Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
        AssistantEntryPoint::kLauncherSearchResult);
    Shell::Get()->assistant_controller()->OpenUrl(
        ash::assistant::util::CreateAssistantQueryDeepLink(
            base::UTF16ToUTF8(result->title())));
  } else {
    if (client_)
      client_->OpenSearchResult(result_id, event_flags);
  }

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::LogResultLaunchHistogram(
    app_list::SearchResultLaunchLocation launch_location,
    int suggestion_index) {
  app_list::RecordSearchLaunchIndexAndQueryLength(
      launch_location, static_cast<int>(last_raw_query_.size()),
      suggestion_index);
}

void AppListControllerImpl::LogSearchClick(
    const std::string& result_id,
    int suggestion_index,
    ash::mojom::AppListLaunchedFrom launched_from) {
  if (client_)
    client_->LogSearchClick(result_id, suggestion_index, launched_from);
}

void AppListControllerImpl::LogSearchAbandonHistogram() {
  app_list::RecordSearchAbandonWithQueryLengthHistogram(GetLastQueryLength());
}

void AppListControllerImpl::InvokeSearchResultAction(
    const std::string& result_id,
    int action_index,
    int event_flags) {
  if (client_)
    client_->InvokeSearchResultAction(result_id, action_index, event_flags);
}

void AppListControllerImpl::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetSearchResultContextMenuModel(result_id, std::move(callback));
}

void AppListControllerImpl::SearchResultContextMenuItemSelected(
    const std::string& result_id,
    int command_id,
    int event_flags) {
  if (client_) {
    client_->SearchResultContextMenuItemSelected(result_id, command_id,
                                                 event_flags);
  }
}

void AppListControllerImpl::ViewShown(int64_t display_id) {
  if (app_list_features::IsEmbeddedAssistantUIEnabled() &&
      GetAssistantViewDelegate()->GetUiModel()->ui_mode() !=
          ash::AssistantUiMode::kLauncherEmbeddedUi) {
    CloseAssistantUi(AssistantExitPoint::kLauncherOpen);
  }
  if (client_)
    client_->ViewShown(display_id);
}

void AppListControllerImpl::ViewClosing() {
  if (presenter_.GetView()->search_box_view()->is_search_box_active())
    LogSearchAbandonHistogram();

  CloseAssistantUi(AssistantExitPoint::kLauncherClose);
  if (client_)
    client_->ViewClosing();
}

void AppListControllerImpl::ViewClosed() {
  // Clear results to prevent initializing the next app list view with outdated
  // results.
  if (client_)
    client_->StartSearch(base::string16());
}

void AppListControllerImpl::GetWallpaperProminentColors(
    GetWallpaperProminentColorsCallback callback) {
  Shell::Get()->wallpaper_controller()->GetWallpaperColors(std::move(callback));
}

void AppListControllerImpl::ActivateItem(const std::string& id,
                                         int event_flags) {
  if (client_)
    client_->ActivateItem(id, event_flags);

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetContextMenuModel(id, std::move(callback));
}

void AppListControllerImpl::ContextMenuItemSelected(const std::string& id,
                                                    int command_id,
                                                    int event_flags) {
  if (client_)
    client_->ContextMenuItemSelected(id, command_id, event_flags);
}

void AppListControllerImpl::ShowWallpaperContextMenu(
    const gfx::Point& onscreen_location,
    ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(onscreen_location, source_type);
}

bool AppListControllerImpl::ProcessHomeLauncherGesture(
    ui::GestureEvent* event,
    const gfx::Point& screen_location) {
  HomeLauncherGestureHandler* home_launcher_gesture_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return home_launcher_gesture_handler->OnPressEvent(
          HomeLauncherGestureHandler::Mode::kSlideDownToHide, screen_location);
    case ui::ET_GESTURE_SCROLL_UPDATE:
      return home_launcher_gesture_handler->OnScrollEvent(
          screen_location, event->details().scroll_y());
    case ui::ET_GESTURE_END:
      return home_launcher_gesture_handler->OnReleaseEvent(screen_location);
    default:
      break;
  }

  NOTREACHED();
  return false;
}

bool AppListControllerImpl::CanProcessEventsOnApplistViews() {
  // Do not allow processing events during overview or while overview is
  // finished but still animating out.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->IsSelecting() ||
      overview_controller->IsCompletingShutdownAnimations()) {
    return false;
  }

  HomeScreenController* home_screen_controller =
      Shell::Get()->home_screen_controller();
  return home_screen_controller &&
         home_screen_controller->home_launcher_gesture_handler()->mode() !=
             HomeLauncherGestureHandler::Mode::kSlideUpToShow;
}

void AppListControllerImpl::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  if (client_)
    client_->GetNavigableContentsFactory(std::move(receiver));
}

ash::AssistantViewDelegate* AppListControllerImpl::GetAssistantViewDelegate() {
  return Shell::Get()->assistant_controller()->view_delegate();
}

void AppListControllerImpl::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {
  if (client_)
    client_->OnSearchResultVisibilityChanged(id, visibility);
}

bool AppListControllerImpl::IsAssistantAllowedAndEnabled() const {
  if (!chromeos::switches::IsAssistantEnabled())
    return false;

  auto* controller = Shell::Get()->voice_interaction_controller();
  return controller->settings_enabled().value_or(false) &&
         controller->allowed_state() == mojom::AssistantAllowedState::ALLOWED;
}

void AppListControllerImpl::AddObserver(AppListControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListControllerImpl::RemoveObserver(
    AppListControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListControllerImpl::NotifyAppListVisibilityChanged(bool visible,
                                                           int64_t display_id) {
  // Notify chrome of visibility changes.
  if (client_)
    client_->OnAppListVisibilityChanged(visible);

  for (auto& observer : observers_)
    observer.OnAppListVisibilityChanged(visible, display_id);
}

void AppListControllerImpl::NotifyAppListTargetVisibilityChanged(bool visible) {
  // Notify chrome of target visibility changes.
  if (client_)
    client_->OnAppListTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// Private used only:

syncer::StringOrdinal AppListControllerImpl::GetOemFolderPos() {
  // Place the OEM folder just after the web store, which should always be
  // followed by a pre-installed app (e.g. Search), so the poosition should be
  // stable. TODO(stevenjb): consider explicitly setting the OEM folder location
  // along with the name in ServicesCustomizationDocument::SetOemFolderName().
  app_list::AppListItemList* item_list = model_->top_level_item_list();
  if (!item_list->item_count()) {
    LOG(ERROR) << "No top level item was found. "
               << "Placing OEM folder at the beginning.";
    return syncer::StringOrdinal::CreateInitialOrdinal();
  }

  size_t web_store_app_index;
  if (!item_list->FindItemIndex(extensions::kWebStoreAppId,
                                &web_store_app_index)) {
    LOG(ERROR) << "Web store position is not found it top items. "
               << "Placing OEM folder at the end.";
    return item_list->item_at(item_list->item_count() - 1)
        ->position()
        .CreateAfter();
  }

  // Skip items with the same position.
  const app_list::AppListItem* web_store_app_item =
      item_list->item_at(web_store_app_index);
  for (size_t j = web_store_app_index + 1; j < item_list->item_count(); ++j) {
    const app_list::AppListItem* next_item = item_list->item_at(j);
    DCHECK(next_item->position().IsValid());
    if (!next_item->position().Equals(web_store_app_item->position())) {
      const syncer::StringOrdinal oem_ordinal =
          web_store_app_item->position().CreateBetween(next_item->position());
      VLOG(1) << "Placing OEM Folder at: " << j
              << " position: " << oem_ordinal.ToDebugString();
      return oem_ordinal;
    }
  }

  const syncer::StringOrdinal oem_ordinal =
      web_store_app_item->position().CreateAfter();
  VLOG(1) << "Placing OEM Folder at: " << item_list->item_count()
          << " position: " << oem_ordinal.ToDebugString();
  return oem_ordinal;
}

std::unique_ptr<app_list::AppListItem> AppListControllerImpl::CreateAppListItem(
    AppListItemMetadataPtr metadata) {
  std::unique_ptr<app_list::AppListItem> app_list_item =
      metadata->is_folder
          ? std::make_unique<app_list::AppListFolderItem>(metadata->id)
          : std::make_unique<app_list::AppListItem>(metadata->id);
  app_list_item->SetMetadata(std::move(metadata));
  return app_list_item;
}

app_list::AppListFolderItem* AppListControllerImpl::FindFolderItem(
    const std::string& folder_id) {
  return model_->FindFolderItem(folder_id);
}

void AppListControllerImpl::UpdateAssistantVisibility() {
  GetSearchModel()->search_box()->SetShowAssistantButton(
      IsAssistantAllowedAndEnabled());
}

int64_t AppListControllerImpl::GetDisplayIdToShowAppListOn() {
  if (IsTabletMode() && !Shell::Get()->display_manager()->IsInUnifiedMode()) {
    return display::Display::HasInternalDisplay()
               ? display::Display::InternalDisplayId()
               : display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(ash::Shell::GetRootWindowForNewWindows())
      .id();
}

void AppListControllerImpl::ResetHomeLauncherIfShown() {
  if (!IsTabletMode() || !presenter_.IsVisible())
    return;

  auto* const keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->IsKeyboardVisible())
    keyboard_controller->HideKeyboardByUser();
  presenter_.GetView()->CloseOpenedPage();

  // Refresh the suggestion chips with empty query.
  StartSearch(base::string16());
}

void AppListControllerImpl::UpdateLauncherContainer() {
  bool launcher_should_show_behind_apps =
      IsTabletMode() &&
      model_->state() != ash::AppListState::kStateEmbeddedAssistant;

  aura::Window* window = presenter_.GetWindow();
  if (!window)
    return;

  auto container_id = launcher_should_show_behind_apps
                          ? ash::kShellWindowId_AppListTabletModeContainer
                          : ash::kShellWindowId_AppListContainer;

  aura::Window* root_window = window->GetRootWindow();
  aura::Window* parent_window = root_window->GetChildById(container_id);
  if (parent_window && !parent_window->Contains(window)) {
    parent_window->AddChild(window);
    bool is_showing_app_window = false;
    for (auto* app_window :
         Shell::Get()->mru_window_tracker()->BuildWindowForCycleList()) {
      if (!parent_window->Contains(app_window) &&
          !wm::GetWindowState(app_window)->IsMinimized()) {
        is_showing_app_window = true;
        break;
      }
    }
    if (launcher_should_show_behind_apps && is_showing_app_window) {
      // When move launcher back to behind apps, and there is app window
      // showing, we release focus.
      Shell::Get()->activation_client()->DeactivateWindow(window);
    }
  }
}

int AppListControllerImpl::GetLastQueryLength() {
  return search_model_.search_box()->text().length();
}

}  // namespace ash
