// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/presenter/app_list_presenter_impl.h"
#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/home_screen/home_launcher_gesture_handler_observer.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/app_list.mojom.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/observer_list.h"
#include "components/sync/model/string_ordinal.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace ui {
class MouseWheelEvent;
}  // namespace ui

namespace ash {

class AppListControllerObserver;

// Ash's AppListController owns the AppListModel and implements interface
// functions that allow Chrome to modify and observe the Shelf and AppListModel
// state.
class ASH_EXPORT AppListControllerImpl
    : public mojom::AppListController,
      public SessionObserver,
      public app_list::AppListModelObserver,
      public app_list::AppListViewDelegate,
      public ash::ShellObserver,
      public OverviewObserver,
      public TabletModeObserver,
      public keyboard::KeyboardControllerObserver,
      public WallpaperControllerObserver,
      public DefaultVoiceInteractionObserver,
      public WindowTreeHostManager::Observer,
      public ash::MruWindowTracker::Observer,
      public AssistantUiModelObserver,
      public HomeLauncherGestureHandlerObserver,
      public HomeScreenDelegate {
 public:
  using AppListItemMetadataPtr = mojom::AppListItemMetadataPtr;
  using SearchResultMetadataPtr = mojom::SearchResultMetadataPtr;
  AppListControllerImpl();
  ~AppListControllerImpl() override;

  // Binds the mojom::AppListController interface request to this object.
  void BindRequest(mojom::AppListControllerRequest request);

  app_list::AppListPresenterImpl* presenter() { return &presenter_; }

  // mojom::AppListController:
  void SetClient(mojom::AppListClientPtr client_ptr) override;
  void AddItem(AppListItemMetadataPtr app_item) override;
  void AddItemToFolder(AppListItemMetadataPtr app_item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  void SetStatus(ash::AppListModelStatus status) override;
  void SetState(ash::AppListState state) override;
  void HighlightItemInstalledFromUI(const std::string& id) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) override;
  void SetSearchHintText(const base::string16& hint_text) override;
  void UpdateSearchBox(const base::string16& text,
                       bool initiated_by_user) override;
  void PublishSearchResults(
      std::vector<SearchResultMetadataPtr> results) override;
  void SetItemMetadata(const std::string& id,
                       AppListItemMetadataPtr data) override;
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemIsInstalling(const std::string& id, bool is_installing) override;
  void SetItemPercentDownloaded(const std::string& id,
                                int32_t percent_downloaded) override;
  void SetModelData(std::vector<AppListItemMetadataPtr> apps,
                    bool is_search_engine_google) override;

  void SetSearchResultMetadata(SearchResultMetadataPtr metadata) override;
  void SetSearchResultIsInstalling(const std::string& id,
                                   bool is_installing) override;
  void SetSearchResultPercentDownloaded(const std::string& id,
                                        int32_t percent_downloaded) override;
  void NotifySearchResultItemInstalled(const std::string& id) override;

  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  void FindOrCreateOemFolder(
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position,
      FindOrCreateOemFolderCallback callback) override;
  void ResolveOemFolderPosition(
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) override;

  void DismissAppList() override;
  void GetAppInfoDialogBounds(GetAppInfoDialogBoundsCallback callback) override;
  void ShowAppListAndSwitchToState(ash::AppListState state) override;
  void ShowAppList() override;

  // app_list::AppListModelObserver:
  void OnAppListItemAdded(app_list::AppListItem* item) override;
  void OnAppListItemWillBeDeleted(app_list::AppListItem* item) override;
  void OnAppListItemUpdated(app_list::AppListItem* item) override;
  void OnAppListStateChanged(ash::AppListState new_state,
                             ash::AppListState old_state) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Methods used in ash:
  bool GetTargetVisibility() const;
  bool IsVisible() const;
  void Show(int64_t display_id,
            app_list::AppListShowSource show_source,
            base::TimeTicks event_time_stamp);
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);
  void EndDragFromShelf(app_list::AppListViewState app_list_state);
  void ProcessMouseWheelEvent(const ui::MouseWheelEvent& event);
  ash::ShelfAction ToggleAppList(int64_t display_id,
                                 app_list::AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp);
  app_list::AppListViewState GetAppListViewState();

  // Called when a window starts/ends dragging. If we're in tablet mode and home
  // launcher is enabled, we should hide the home launcher during dragging a
  // window and reshow it when the drag ends.
  void OnWindowDragStarted();
  void OnWindowDragEnded();

  // app_list::AppListViewDelegate:
  app_list::AppListModel* GetModel() override;
  app_list::SearchModel* GetSearchModel() override;
  void StartAssistant() override;
  void StartSearch(const base::string16& raw_query) override;
  void OpenSearchResult(const std::string& result_id, int event_flags) override;
  void LogSearchClick(const std::string& result_id,
                      int suggestion_index,
                      ash::mojom::AppListLaunchedFrom launched_from) override;
  void LogResultLaunchHistogram(
      app_list::SearchResultLaunchLocation launch_location,
      int suggestion_index) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  using GetContextMenuModelCallback =
      AppListViewDelegate::GetContextMenuModelCallback;
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void SearchResultContextMenuItemSelected(const std::string& result_id,
                                           int command_id,
                                           int event_flags) override;
  void ViewShown(int64_t display_id) override;
  void ViewClosing() override;
  void ViewClosed() override;
  void GetWallpaperProminentColors(
      GetWallpaperProminentColorsCallback callback) override;
  void ActivateItem(const std::string& id, int event_flags) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override;
  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override;
  bool ProcessHomeLauncherGesture(ui::GestureEvent* event,
                                  const gfx::Point& screen_location) override;
  bool CanProcessEventsOnApplistViews() override;
  void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver)
      override;
  ash::AssistantViewDelegate* GetAssistantViewDelegate() override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override;

  void AddObserver(AppListControllerObserver* observer);
  void RemoveObserver(AppListControllerObserver* obsever);

  // AppList visibility announcements are for clamshell mode AppList.
  void NotifyAppListVisibilityChanged(bool visible, int64_t display_id);
  void NotifyAppListTargetVisibilityChanged(bool visible);

  void FlushForTesting();

  // ShellObserver:
  void OnShellDestroying() override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityStateChanged(bool is_visible) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;
  void OnWallpaperPreviewStarted() override;
  void OnWallpaperPreviewEnded() override;

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // MruWindowTracker::Observer:
  void OnWindowUntracked(aura::Window* untracked_window) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // HomeLauncherGestureHandlerObserver:
  void OnHomeLauncherAnimationComplete(bool shown, int64_t display_id) override;

  // HomeScreenDelegate:
  void UpdateYPositionAndOpacityForHomeLauncher(
      int y_position_in_screen,
      float opacity,
      UpdateAnimationSettingsCallback callback) override;
  void UpdateAfterHomeLauncherShown() override;
  base::Optional<base::TimeDelta> GetOptionalAnimationDuration() override;

  bool onscreen_keyboard_shown() const { return onscreen_keyboard_shown_; }

  // Performs the 'back' action for the active page.
  void Back();

  // Handles app list button press event. (Search key should trigger the same
  // behavior.) All three parameters are only used in clamshell mode.
  // |display_id| is the id of display where app list should toggle.
  // |show_source| is the source of the event. |event_time_stamp| records the
  // event timestamp.
  ash::ShelfAction OnAppListButtonPressed(
      int64_t display_id,
      app_list::AppListShowSource show_source,
      base::TimeTicks event_time_stamp);

  // Returns current visibility of the Assistant page.
  bool IsShowingEmbeddedAssistantUI() const;

  // Get updated app list view state after dragging from shelf.
  app_list::AppListViewState CalculateStateAfterShelfDrag(
      const ui::GestureEvent& gesture_in_screen,
      float launcher_above_shelf_bottom_amount) const;

  void SetAppListModelForTest(std::unique_ptr<app_list::AppListModel> model);

 private:
  syncer::StringOrdinal GetOemFolderPos();
  std::unique_ptr<app_list::AppListItem> CreateAppListItem(
      AppListItemMetadataPtr metadata);
  app_list::AppListFolderItem* FindFolderItem(const std::string& folder_id);

  // Update the visibility of the home launcher based on e.g. if the device is
  // in overview mode.
  void UpdateHomeLauncherVisibility();

  // Update the visibility of Assistant functionality.
  void UpdateAssistantVisibility();

  // Updates the visibility of expand arrow view.
  void UpdateExpandArrowVisibility();

  int64_t GetDisplayIdToShowAppListOn();

  // Shows the home launcher in tablet mode.
  void ShowHomeLauncher();

  void ResetHomeLauncherIfShown();

  // Updates which container the launcher window should be in.
  void UpdateLauncherContainer();

  base::string16 last_raw_query_;

  mojom::AppListClientPtr client_;

  std::unique_ptr<app_list::AppListModel> model_;
  app_list::SearchModel search_model_;

  // |presenter_| should be put below |client_| and |model_| to prevent a crash
  // in destruction.
  app_list::AppListPresenterImpl presenter_;

  // Bindings for the AppListController interface.
  mojo::BindingSet<mojom::AppListController> bindings_;

  // Whether the on-screen keyboard is shown.
  bool onscreen_keyboard_shown_ = false;

  // Each time overview mode is exited, set this variable based on whether
  // overview mode is sliding out, so the home launcher knows what to do when
  // overview mode exit animations are finished.
  bool use_slide_to_exit_overview_ = false;

  // Whether the wallpaper is being previewed. The home launcher (if enabled)
  // should be hidden during wallpaper preview.
  bool in_wallpaper_preview_ = false;

  // Whether we're currently in a window dragging process.
  bool in_window_dragging_ = false;

  base::ObserverList<AppListControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
