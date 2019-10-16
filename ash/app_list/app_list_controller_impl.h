// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/home_screen/home_launcher_gesture_handler_observer.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/observer_list.h"
#include "components/sync/model/string_ordinal.h"
#include "ui/display/types/display_constants.h"

class PrefRegistrySimple;

namespace ui {
class MouseWheelEvent;
}  // namespace ui

namespace ash {

class AppListControllerObserver;

// Ash's AppListController owns the AppListModel and implements interface
// functions that allow Chrome to modify and observe the Shelf and AppListModel
// state.
class ASH_EXPORT AppListControllerImpl
    : public AppListController,
      public SessionObserver,
      public AppListModelObserver,
      public AppListViewDelegate,
      public ash::ShellObserver,
      public OverviewObserver,
      public TabletModeObserver,
      public KeyboardControllerObserver,
      public WallpaperControllerObserver,
      public AssistantStateObserver,
      public WindowTreeHostManager::Observer,
      public ash::MruWindowTracker::Observer,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public HomeLauncherGestureHandlerObserver,
      public HomeScreenDelegate {
 public:
  AppListControllerImpl();
  ~AppListControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AppListPresenterImpl* presenter() { return &presenter_; }

  // AppListController:
  void SetClient(AppListClient* client) override;
  AppListClient* GetClient() override;
  void AddItem(std::unique_ptr<ash::AppListItemMetadata> app_item) override;
  void AddItemToFolder(std::unique_ptr<ash::AppListItemMetadata> app_item,
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
      std::vector<std::unique_ptr<ash::SearchResultMetadata>> results) override;
  void SetItemMetadata(const std::string& id,
                       std::unique_ptr<ash::AppListItemMetadata> data) override;
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemIsInstalling(const std::string& id, bool is_installing) override;
  void SetItemPercentDownloaded(const std::string& id,
                                int32_t percent_downloaded) override;
  void SetModelData(int profile_id,
                    std::vector<std::unique_ptr<ash::AppListItemMetadata>> apps,
                    bool is_search_engine_google) override;

  void SetSearchResultMetadata(
      std::unique_ptr<ash::SearchResultMetadata> metadata) override;
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
  void ShowAppList() override;
  aura::Window* GetWindow() override;
  bool IsVisible() override;

  // AppListModelObserver:
  void OnAppListItemAdded(AppListItem* item) override;
  void OnAppListItemWillBeDeleted(AppListItem* item) override;
  void OnAppListItemUpdated(AppListItem* item) override;
  void OnAppListStateChanged(ash::AppListState new_state,
                             ash::AppListState old_state) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Methods used in ash:
  bool GetTargetVisibility() const;
  void Show(int64_t display_id,
            AppListShowSource show_source,
            base::TimeTicks event_time_stamp);
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);
  void EndDragFromShelf(ash::AppListViewState app_list_state);
  void ProcessMouseWheelEvent(const ui::MouseWheelEvent& event);
  ash::ShelfAction ToggleAppList(int64_t display_id,
                                 AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp);
  ash::AppListViewState GetAppListViewState();

  // AppListViewDelegate:
  AppListModel* GetModel() override;
  SearchModel* GetSearchModel() override;
  void StartAssistant() override;
  void StartSearch(const base::string16& raw_query) override;
  void OpenSearchResult(const std::string& result_id,
                        int event_flags,
                        AppListLaunchedFrom launched_from,
                        AppListLaunchType launch_type,
                        int suggestion_index) override;
  void LogResultLaunchHistogram(SearchResultLaunchLocation launch_location,
                                int suggestion_index) override;
  void LogSearchAbandonHistogram() override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  using GetContextMenuModelCallback =
      AppListViewDelegate::GetContextMenuModelCallback;
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewShown(int64_t display_id) override;
  void ViewClosing() override;
  void ViewClosed() override;
  const std::vector<SkColor>& GetWallpaperProminentColors() override;
  void ActivateItem(const std::string& id,
                    int event_flags,
                    AppListLaunchedFrom launched_from) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  ui::ImplicitAnimationObserver* GetAnimationObserver(
      ash::AppListViewState target_state) override;
  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override;
  bool ProcessHomeLauncherGesture(ui::GestureEvent* event,
                                  const gfx::Point& screen_location) override;
  bool KeyboardTraversalEngaged() override;
  bool CanProcessEventsOnApplistViews() override;
  bool ShouldDismissImmediately() override;
  void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver)
      override;
  int GetTargetYForAppListHide(aura::Window* root_window) override;
  ash::AssistantViewDelegate* GetAssistantViewDelegate() override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override;
  void NotifySearchResultsForLogging(
      const base::string16& raw_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int position_index) override;
  bool IsAssistantAllowedAndEnabled() const override;
  bool ShouldShowAssistantPrivacyInfo() const override;
  void MaybeIncreaseAssistantPrivacyInfoShownCount() override;
  void MarkAssistantPrivacyInfoDismissed() override;
  void OnStateTransitionAnimationCompleted(
      ash::AppListViewState state) override;
  void GetAppLaunchedMetricParams(
      AppLaunchedMetricParams* metric_params) override;
  gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds) override;
  int GetShelfHeight() override;

  void AddObserver(AppListControllerObserver* observer);
  void RemoveObserver(AppListControllerObserver* obsever);

  // Notifies observers of AppList visibility changes.
  void OnVisibilityChanged(bool visible, int64_t display_id);
  void OnVisibilityWillChange(bool visible, int64_t display_id);

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;
  void OnShellDestroying() override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  // AssistantStateObserver:
  void OnAssistantStatusChanged(mojom::AssistantState state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // MruWindowTracker::Observer:
  void OnWindowUntracked(aura::Window* untracked_window) override;

  // AssistantControllerObserver:
  void OnAssistantReady() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // HomeLauncherGestureHandlerObserver:
  void OnHomeLauncherAnimationComplete(bool shown, int64_t display_id) override;
  void OnHomeLauncherTargetPositionChanged(bool showing,
                                           int64_t display_id) override;

  // HomeScreenDelegate:
  void ShowHomeScreenView() override;
  aura::Window* GetHomeScreenWindow() override;
  void UpdateYPositionAndOpacityForHomeLauncher(
      int y_position_in_screen,
      float opacity,
      UpdateAnimationSettingsCallback callback) override;
  void UpdateAfterHomeLauncherShown() override;
  base::Optional<base::TimeDelta> GetOptionalAnimationDuration() override;
  void NotifyHomeLauncherAnimationTransition(AnimationTrigger trigger,
                                             bool launcher_will_show) override;
  bool IsHomeScreenVisible() override;

  bool onscreen_keyboard_shown() const { return onscreen_keyboard_shown_; }

  // Performs the 'back' action for the active page.
  void Back();

  void SetKeyboardTraversalMode(bool engaged);

  // Handles home button press event. (Search key should trigger the same
  // behavior.) All three parameters are only used in clamshell mode.
  // |display_id| is the id of display where app list should toggle.
  // |show_source| is the source of the event. |event_time_stamp| records the
  // event timestamp.
  ash::ShelfAction OnHomeButtonPressed(int64_t display_id,
                                       AppListShowSource show_source,
                                       base::TimeTicks event_time_stamp);

  // Returns current visibility of the Assistant page.
  bool IsShowingEmbeddedAssistantUI() const;

  // Get updated app list view state after dragging from shelf.
  ash::AppListViewState CalculateStateAfterShelfDrag(
      const ui::LocatedEvent& event_in_screen,
      float launcher_above_shelf_bottom_amount) const;

  void SetAppListModelForTest(std::unique_ptr<AppListModel> model);

  using StateTransitionAnimationCallback =
      base::RepeatingCallback<void(ash::AppListViewState)>;

  void SetStateTransitionAnimationCallback(
      StateTransitionAnimationCallback callback);

  void RecordShelfAppLaunched(
      base::Optional<AppListViewState> recorded_app_list_view_state,
      base::Optional<bool> home_launcher_shown);

  // Updates which container the launcher window should be in.
  void UpdateLauncherContainer(
      base::Optional<int64_t> display_id = base::nullopt);

  // Gets the container which should contain the AppList.
  int GetContainerId() const;

  // Returns whether the launcher should show behinds apps or infront of them.
  bool ShouldLauncherShowBehindApps() const;

  // Returns the parent window of the applist for a |display_id|.
  aura::Window* GetContainerForDisplayId(
      base::Optional<int64_t> display_id = base::nullopt);

 private:
  // HomeScreenDelegate:
  void OnHomeLauncherDragStart() override;
  void OnHomeLauncherDragInProgress() override;
  void OnHomeLauncherDragEnd() override;

  syncer::StringOrdinal GetOemFolderPos();
  std::unique_ptr<AppListItem> CreateAppListItem(
      std::unique_ptr<ash::AppListItemMetadata> metadata);
  AppListFolderItem* FindFolderItem(const std::string& folder_id);

  // Update the visibility of Assistant functionality.
  void UpdateAssistantVisibility();

  // Updates the visibility of expand arrow view.
  void UpdateExpandArrowVisibility();

  int64_t GetDisplayIdToShowAppListOn();

  void ResetHomeLauncherIfShown();

  // Returns the length of the most recent query.
  int GetLastQueryLength();

  // Shuts down the AppListControllerImpl, removing itself as an observer.
  void Shutdown();

  // Record the app launch for AppListAppLaunchedV2 metric.
  void RecordAppLaunched(AppListLaunchedFrom launched_from);

  AppListClient* client_ = nullptr;

  std::unique_ptr<AppListModel> model_;
  SearchModel search_model_;

  // |presenter_| should be put below |client_| and |model_| to prevent a crash
  // in destruction.
  AppListPresenterImpl presenter_;

  // True if the on-screen keyboard is shown.
  bool onscreen_keyboard_shown_ = false;

  // True if the most recent event handled by |presenter_| was a key event.
  bool keyboard_traversal_engaged_ = false;

  // True if Shutdown() has been called.
  bool is_shutdown_ = false;

  // Whether to immediately dismiss the AppListView.
  bool should_dismiss_immediately_ = false;

  // Whether the home launcher is in the process of being animated into view.
  // This becomes true at the start of the animation (or the drag), becomes
  // false once it ends and stays false until the next animation or drag
  // showing the home launcher.
  bool animation_or_drag_to_visible_home_launcher_in_progress_ = false;

  // The last target visibility change and its display id.
  bool last_target_visible_ = false;
  int64_t last_target_visible_display_id_ = display::kInvalidDisplayId;

  // The last visibility change and its display id.
  bool last_visible_ = false;
  int64_t last_visible_display_id_ = display::kInvalidDisplayId;

  // Used in mojo callings to specify the profile whose app list data is
  // read/written by Ash side through IPC. Notice that in multi-profile mode,
  // each profile has its own AppListModelUpdater to manipulate app list items.
  int profile_id_ = kAppListInvalidProfileID;

  StateTransitionAnimationCallback state_transition_animation_callback_;

  base::ObserverList<AppListControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
