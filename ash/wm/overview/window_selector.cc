// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility_types.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/splitview/split_view_overview_overlay.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// The amount of padding surrounding the text in the text filtering textbox.
const int kTextFilterHorizontalPadding = 10;

// The height of the text filtering textbox.
const int kTextFilterHeight = 40;

// The margin at the bottom to make sure the text filter layer is hidden.
// This is needed because positioning the text filter directly touching the top
// edge of the screen still allows the shadow to peek through.
const int kTextFieldBottomMargin = 2;

// Distance from top of overview to the top of text filtering textbox as a
// proportion of the total overview area.
const float kTextFilterTopScreenProportion = 0.02f;

// Width of the text filter area.
const int kTextFilterWidth = 280;

// The font style used for text filtering textbox.
static const ui::ResourceBundle::FontStyle kTextFilterFontStyle =
    ui::ResourceBundle::FontStyle::BaseFont;

// The color of the text and its background in the text filtering textbox.
const SkColor kTextFilterTextColor = SkColorSetARGB(222, 0, 0, 0);
const SkColor kTextFilterBackgroundColor = SK_ColorWHITE;

// The color or search icon.
const SkColor kTextFilterIconColor = SkColorSetARGB(138, 0, 0, 0);

// The size of search icon.
const int kTextFilterIconSize = 20;

// The radius used for the rounded corners on the text filtering textbox.
const int kTextFilterCornerRadius = 2;

// A comparator for locating a selector item for a given root.
struct WindowSelectorItemForRoot {
  explicit WindowSelectorItemForRoot(const aura::Window* root)
      : root_window(root) {}

  bool operator()(WindowSelectorItem* item) const {
    return item->root_window() == root_window;
  }

  const aura::Window* root_window;
};

// A View having rounded corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
// TODO(tdanderson): This duplicates code from RoundedImageView. Refactor these
//                   classes and move into ui/views.
class RoundedContainerView : public views::View {
 public:
  RoundedContainerView(int corner_radius, SkColor background)
      : corner_radius_(corner_radius), background_(background) {}

  ~RoundedContainerView() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    SkScalar radius = SkIntToScalar(corner_radius_);
    const SkScalar kRadius[8] = {radius, radius, radius, radius,
                                 radius, radius, radius, radius};
    SkPath path;
    gfx::Rect bounds(size());
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

    canvas->ClipPath(path, true);
    canvas->DrawColor(background_);
  }

 private:
  int corner_radius_;
  SkColor background_;

  DISALLOW_COPY_AND_ASSIGN(RoundedContainerView);
};

// Triggers a shelf visibility update on all root window controllers.
void UpdateShelfVisibility() {
  for (aura::Window* root : Shell::GetAllRootWindows())
    Shelf::ForWindow(root)->UpdateVisibilityState();
}

// Returns the bounds for the overview window grid according to the split view
// state. If split view mode is active, the overview window should open on the
// opposite side of the default snap window.
gfx::Rect GetGridBoundsInScreen(aura::Window* root_window) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (split_view_controller->IsSplitViewModeActive()) {
    SplitViewController::SnapPosition oppsite_position =
        (split_view_controller->default_snap_position() ==
         SplitViewController::LEFT)
            ? SplitViewController::RIGHT
            : SplitViewController::LEFT;
    return split_view_controller->GetSnappedWindowBoundsInScreen(
        root_window, oppsite_position);
  } else {
    return split_view_controller->GetDisplayWorkAreaBoundsInScreen(root_window);
  }
}

gfx::Rect GetTextFilterPosition(aura::Window* root_window) {
  const gfx::Rect total_bounds = GetGridBoundsInScreen(root_window);
  return gfx::Rect(
      total_bounds.x() +
          0.5 * (total_bounds.width() -
                 std::min(kTextFilterWidth, total_bounds.width())),
      total_bounds.y() + total_bounds.height() * kTextFilterTopScreenProportion,
      std::min(kTextFilterWidth, total_bounds.width()), kTextFilterHeight);
}

// Initializes the text filter on the top of the main root window and requests
// focus on its textfield. Uses |image| to place an icon to the left of the text
// field.
views::Widget* CreateTextFilter(views::TextfieldController* controller,
                                aura::Window* root_window,
                                const gfx::ImageSkia& image,
                                int* text_filter_bottom) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.bounds = GetTextFilterPosition(root_window);
  params.name = "OverviewModeTextFilter";
  *text_filter_bottom = params.bounds.bottom() + kTextFieldBottomMargin;
  params.parent = root_window->GetChildById(kShellWindowId_StatusContainer);
  widget->Init(params);

  // Use |container| to specify the padding surrounding the text and to give
  // the textfield rounded corners.
  views::View* container = new RoundedContainerView(kTextFilterCornerRadius,
                                                    kTextFilterBackgroundColor);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  const int text_height =
      std::max(kTextFilterIconSize,
               bundle.GetFontList(kTextFilterFontStyle).GetHeight());
  DCHECK(text_height);
  const int vertical_padding = (params.bounds.height() - text_height) / 2;
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal,
      gfx::Insets(vertical_padding, kTextFilterHorizontalPadding),
      kTextFilterHorizontalPadding);
  container->SetLayoutManager(layout);

  views::Textfield* textfield = new views::Textfield;
  textfield->set_controller(controller);
  textfield->SetBorder(views::NullBorder());
  textfield->SetBackgroundColor(kTextFilterBackgroundColor);
  textfield->SetTextColor(kTextFilterTextColor);
  views::ImageView* image_view = new views::ImageView;
  image_view->SetImage(image);
  container->AddChildView(image_view);
  textfield->SetFontList(bundle.GetFontList(kTextFilterFontStyle));
  container->AddChildView(textfield);
  layout->SetFlexForView(textfield, 1);
  widget->SetContentsView(container);

  // The textfield initially contains no text, so shift its position to be
  // outside the visible bounds of the screen.
  gfx::Transform transform;
  transform.Translate(0, -(*text_filter_bottom));
  aura::Window* text_filter_widget_window = widget->GetNativeWindow();
  text_filter_widget_window->layer()->SetOpacity(0);
  text_filter_widget_window->SetTransform(transform);
  widget->Show();
  textfield->RequestFocus();

  return widget;
}

}  // namespace

// static
bool WindowSelector::IsSelectable(aura::Window* window) {
  return wm::GetWindowState(window)->IsUserPositionable();
}

WindowSelector::WindowSelector(WindowSelectorDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(wm::GetFocusedWindow()),
      ignore_activations_(false),
      selected_grid_index_(0),
      overview_start_time_(base::Time::Now()),
      num_key_presses_(0),
      num_items_(0),
      showing_text_filter_(false),
      text_filter_string_length_(0),
      num_times_textfield_cleared_(0),
      restoring_minimized_windows_(false),
      text_filter_bottom_(0) {
  DCHECK(delegate_);
}

WindowSelector::~WindowSelector() {
  DCHECK(observed_windows_.empty());
  // Don't delete |window_drag_controller_| yet since the stack might be still
  // using it.
  if (window_drag_controller_) {
    window_drag_controller_->ResetWindowSelector();
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, window_drag_controller_.release());
  }
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, (ie PanelLayoutManager) to make indirect
// calls to restoring_minimized_windows() on a partially constructed object.
void WindowSelector::Init(const WindowList& windows,
                          const WindowList& hide_windows) {
  hide_overview_windows_ =
      std::make_unique<ScopedHideOverviewWindows>(std::move(hide_windows));
  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  if (SplitViewController::ShouldAllowSplitView())
    split_view_overview_overlay_ = std::make_unique<SplitViewOverviewOverlay>();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const aura::Window* a, const aura::Window* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (auto* root : root_windows) {
    // Observed switchable containers for newly created windows on all root
    // windows.
    for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container =
          root->GetChildById(wm::kSwitchableWindowContainerIds[i]);
      container->AddObserver(this);
      observed_windows_.insert(container);
    }

    // Hide the callout widgets for panels. It is safe to call this for
    // root windows that don't contain any panel windows.
    PanelLayoutManager::Get(root)->SetShowCalloutWidgets(false);

    std::unique_ptr<WindowGrid> grid(
        new WindowGrid(root, windows, this, GetGridBoundsInScreen(root)));
    if (grid->empty())
      continue;
    num_items_ += grid->size();
    grid_list_.push_back(std::move(grid));
  }

  {
    // The calls to WindowGrid::PrepareForOverview() and CreateTextFilter(...)
    // requires some LayoutManagers (ie PanelLayoutManager) to perform layouts
    // so that windows are correctly visible and properly animated in overview
    // mode. Otherwise these layouts should be suppressed during overview mode
    // so they don't conflict with overview mode animations. The
    // |restoring_minimized_windows_| flag enables the PanelLayoutManager to
    // make this decision.
    base::AutoReset<bool> auto_restoring_minimized_windows(
        &restoring_minimized_windows_, true);

    // Do not call PrepareForOverview until all items are added to window_list_
    // as we don't want to cause any window updates until all windows in
    // overview are observed. See http://crbug.com/384495.
    for (std::unique_ptr<WindowGrid>& window_grid : grid_list_) {
      window_grid->PrepareForOverview();
      window_grid->PositionWindows(true);
    }

    search_image_ = gfx::CreateVectorIcon(
        vector_icons::kSearchIcon, kTextFilterIconSize, kTextFilterIconColor);
    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    text_filter_widget_.reset(CreateTextFilter(this, root_window, search_image_,
                                               &text_filter_bottom_));
  }

  DCHECK(!grid_list_.empty());
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", num_items_);

  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->split_view_controller()->AddObserver(this);

  display::Screen::GetScreen()->AddObserver(this);
  base::RecordAction(base::UserMetricsAction("WindowSelector_Overview"));
  // Send an a11y alert.
  Shell::Get()->accessibility_delegate()->TriggerAccessibilityAlert(
      A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);

  UpdateShelfVisibility();
}

// NOTE: The work done in Shutdown() is not done in the destructor because it
// may cause other, unrelated classes, (ie PanelLayoutManager) to make indirect
// calls to restoring_minimized_windows() on a partially destructed object.
void WindowSelector::Shutdown() {
  is_shut_down_ = true;
  // Stop observing screen metrics changes first to avoid auto-positioning
  // windows in response to work area changes from window activation.
  display::Screen::GetScreen()->RemoveObserver(this);

  // Stop observing split view state changes before restoring window focus.
  // Otherwise the activation of the window triggers OnSplitViewStateChanged()
  // that will call into this function again.
  Shell::Get()->split_view_controller()->RemoveObserver(this);

  size_t remaining_items = 0;
  for (std::unique_ptr<WindowGrid>& window_grid : grid_list_) {
    for (const auto& window_selector_item : window_grid->window_list())
      window_selector_item->RestoreWindow();
    remaining_items += window_grid->size();
  }

  // Setting focus after restoring windows' state avoids unnecessary animations.
  ResetFocusRestoreWindow(true);
  RemoveAllObservers();

  for (aura::Window* window : Shell::GetAllRootWindows()) {
    // Un-hide the callout widgets for panels. It is safe to call this for
    // root_windows that don't contain any panel windows.
    PanelLayoutManager::Get(window)->SetShowCalloutWidgets(true);
  }

  for (std::unique_ptr<WindowGrid>& window_grid : grid_list_)
    window_grid->Shutdown();

  DCHECK(num_items_ >= remaining_items);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.OverviewClosedItems",
                           num_items_ - remaining_items);
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowSelector.TimeInOverview",
                             base::Time::Now() - overview_start_time_);

  // Record metrics related to text filtering.
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.TextFilteringStringLength",
                           text_filter_string_length_);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.TextFilteringTextfieldCleared",
                           num_times_textfield_cleared_);
  if (text_filter_string_length_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Ash.WindowSelector.TimeInOverviewWithTextFiltering",
        base::Time::Now() - overview_start_time_);
    UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ItemsWhenTextFilteringUsed",
                             remaining_items);
  }

  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  grid_list_.clear();
  UpdateShelfVisibility();
}

void WindowSelector::RemoveAllObservers() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  observed_windows_.clear();

  Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  if (restore_focus_window_)
    restore_focus_window_->RemoveObserver(this);
}

void WindowSelector::CancelSelection() {
  delegate_->OnSelectionEnded();
}

void WindowSelector::OnGridEmpty(WindowGrid* grid) {
  size_t index = 0;
  for (auto iter = grid_list_.begin(); iter != grid_list_.end(); ++iter) {
    if (grid == (*iter).get()) {
      index = iter - grid_list_.begin();
      grid_list_.erase(iter);
      break;
    }
  }
  if (index > 0 && selected_grid_index_ >= index) {
    selected_grid_index_--;
    // If the grid which became empty was the one with the selected window, we
    // need to select a window on the newly selected grid.
    if (selected_grid_index_ == index - 1)
      Move(LEFT, true);
  }
  if (grid_list_.empty())
    CancelSelection();
}

void WindowSelector::IncrementSelection(int increment) {
  const Direction direction =
      increment > 0 ? WindowSelector::RIGHT : WindowSelector::LEFT;
  for (int step = 0; step < abs(increment); ++step)
    Move(direction, true);
}

bool WindowSelector::AcceptSelection() {
  if (!grid_list_[selected_grid_index_]->is_selecting())
    return false;
  SelectWindow(grid_list_[selected_grid_index_]->SelectedWindow());
  return true;
}

void WindowSelector::SelectWindow(WindowSelectorItem* item) {
  aura::Window* window = item->GetWindow();
  aura::Window::Windows window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  if (!window_list.empty()) {
    // Record WindowSelector_ActiveWindowChanged if the user is selecting a
    // window other than the window that was active prior to entering overview
    // mode (i.e., the window at the front of the MRU list).
    if (window_list[0] != window) {
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::OVERVIEW_MODE);
    }
    const auto it = std::find(window_list.begin(), window_list.end(), window);
    if (it != window_list.end()) {
      // Record 1-based index so that selecting a top MRU window will record 1.
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.SelectionDepth",
                               1 + it - window_list.begin());
    }
  }
  item->EnsureVisible();
  wm::GetWindowState(window)->Activate();
}

void WindowSelector::WindowClosing(WindowSelectorItem* window) {
  grid_list_[selected_grid_index_]->WindowClosing(window);
}

void WindowSelector::SetBoundsForWindowGridsInScreen(const gfx::Rect& bounds) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->SetBoundsAndUpdatePositions(bounds);
}

void WindowSelector::SetBoundsForWindowGridsInScreenIgnoringWindow(
    const gfx::Rect& bounds,
    WindowSelectorItem* ignored_item) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->SetBoundsAndUpdatePositionsIgnoringWindow(bounds, ignored_item);
}

void WindowSelector::SetSplitViewOverviewOverlayIndicatorType(
    IndicatorType indicator_type,
    const gfx::Point& event_location) {
  DCHECK(split_view_overview_overlay_);
  split_view_overview_overlay_->SetIndicatorType(indicator_type,
                                                 event_location);
}

WindowGrid* WindowSelector::GetGridWithRootWindow(aura::Window* root_window) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    if (grid->root_window() == root_window)
      return grid.get();
  }

  return nullptr;
}

void WindowSelector::RemoveWindowSelectorItem(WindowSelectorItem* item) {
  if (item->GetWindow()->HasObserver(this)) {
    item->GetWindow()->RemoveObserver(this);
    observed_windows_.erase(item->GetWindow());
    if (item->GetWindow() == restore_focus_window_)
      restore_focus_window_ = nullptr;
  }

  // Remove |item| from the corresponding grid.
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    if (grid->Contains(item->GetWindow())) {
      grid->RemoveItem(item);
      if (grid->empty())
        OnGridEmpty(grid.get());
      break;
    }
  }
}

void WindowSelector::InitiateDrag(WindowSelectorItem* item,
                                  const gfx::Point& location_in_screen) {
  window_drag_controller_.reset(new OverviewWindowDragController(this));
  window_drag_controller_->InitiateDrag(item, location_in_screen);
}

void WindowSelector::Drag(WindowSelectorItem* item,
                          const gfx::Point& location_in_screen) {
  DCHECK(window_drag_controller_.get());
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->Drag(location_in_screen);
}

void WindowSelector::CompleteDrag(WindowSelectorItem* item,
                                  const gfx::Point& location_in_screen) {
  DCHECK(window_drag_controller_.get());
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->CompleteDrag(location_in_screen);
}

void WindowSelector::PositionWindows(bool animate) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->PositionWindows(animate);
}

bool WindowSelector::HandleKeyEvent(views::Textfield* sender,
                                    const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  switch (key_event.key_code()) {
    case ui::VKEY_BROWSER_BACK:
    case ui::VKEY_ESCAPE:
      CancelSelection();
      break;
    case ui::VKEY_UP:
      num_key_presses_++;
      Move(WindowSelector::UP, true);
      break;
    case ui::VKEY_DOWN:
      num_key_presses_++;
      Move(WindowSelector::DOWN, true);
      break;
    case ui::VKEY_RIGHT:
    case ui::VKEY_TAB:
      if (key_event.key_code() == ui::VKEY_RIGHT ||
          !(key_event.flags() & ui::EF_SHIFT_DOWN)) {
        num_key_presses_++;
        Move(WindowSelector::RIGHT, true);
        break;
      }
    case ui::VKEY_LEFT:
      num_key_presses_++;
      Move(WindowSelector::LEFT, true);
      break;
    case ui::VKEY_W:
      if (!(key_event.flags() & ui::EF_CONTROL_DOWN) ||
          !grid_list_[selected_grid_index_]->is_selecting()) {
        // Allow the textfield to handle 'W' key when not used with Ctrl.
        return false;
      }
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewCloseKey"));
      grid_list_[selected_grid_index_]->SelectedWindow()->CloseWindow();
      break;
    case ui::VKEY_RETURN:
      // Ignore if no item is selected.
      if (!grid_list_[selected_grid_index_]->is_selecting())
        return false;
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ArrowKeyPresses",
                               num_key_presses_);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.WindowSelector.KeyPressesOverItemsRatio",
                                  (num_key_presses_ * 100) / num_items_, 1, 300,
                                  30);
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
      SelectWindow(grid_list_[selected_grid_index_]->SelectedWindow());
      break;
    default:
      // Not a key we are interested in, allow the textfield to handle it.
      return false;
  }
  return true;
}

void WindowSelector::OnDisplayAdded(const display::Display& display) {}

void WindowSelector::OnDisplayRemoved(const display::Display& display) {
  // TODO(flackr): Keep window selection active on remaining displays.
  CancelSelection();
}

void WindowSelector::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t metrics) {
  // For metrics changes that happen when the split view mode is active, the
  // display bounds will be adjusted in OnSplitViewDividerPositionChanged().
  if (Shell::Get()->IsSplitViewModeActive())
    return;
  OnDisplayBoundsChanged();
}

void WindowSelector::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Only care about newly added children of |observed_windows_|.
  if (!observed_windows_.count(params.receiver) ||
      !observed_windows_.count(params.new_parent)) {
    return;
  }

  aura::Window* new_window = params.target;
  if (!IsSelectable(new_window))
    return;

  for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
    if (new_window->parent()->id() == wm::kSwitchableWindowContainerIds[i] &&
        !::wm::GetTransientParent(new_window)) {
      // The new window is in one of the switchable containers, abort overview.
      CancelSelection();
      return;
    }
  }
}

void WindowSelector::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = nullptr;
}

void WindowSelector::OnWindowActivated(ActivationReason reason,
                                       aura::Window* gained_active,
                                       aura::Window* lost_active) {
  if (ignore_activations_ || !gained_active ||
      gained_active == GetTextFilterWidgetWindow()) {
    return;
  }

  aura::Window* root_window = gained_active->GetRootWindow();
  auto grid =
      std::find_if(grid_list_.begin(), grid_list_.end(),
                   [root_window](const std::unique_ptr<WindowGrid>& grid) {
                     return grid->root_window() == root_window;
                   });
  if (grid == grid_list_.end())
    return;
  const auto& windows = (*grid)->window_list();

  auto iter = std::find_if(
      windows.begin(), windows.end(),
      [gained_active](const std::unique_ptr<WindowSelectorItem>& window) {
        return window->Contains(gained_active);
      });

  if (iter == windows.end() && showing_text_filter_ &&
      lost_active == GetTextFilterWidgetWindow()) {
    return;
  }

  // Do not cancel the overview mode if the window activation was caused by
  // snapping window to one side of the screen.
  if (Shell::Get()->IsSplitViewModeActive())
    return;

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::OnAttemptToReactivateWindow(aura::Window* request_active,
                                                 aura::Window* actual_active) {
  OnWindowActivated(ActivationReason::ACTIVATION_CLIENT, request_active,
                    actual_active);
}

void WindowSelector::ContentsChanged(views::Textfield* sender,
                                     const base::string16& new_contents) {
  text_filter_string_length_ = new_contents.length();
  if (!text_filter_string_length_)
    num_times_textfield_cleared_++;

  bool should_show_text_filter = !new_contents.empty();
  if (showing_text_filter_ != should_show_text_filter) {
    aura::Window* text_filter_widget_window = GetTextFilterWidgetWindow();
    ui::ScopedLayerAnimationSettings animation_settings(
        text_filter_widget_window->layer()->GetAnimator());
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTweenType(showing_text_filter_
                                        ? gfx::Tween::FAST_OUT_LINEAR_IN
                                        : gfx::Tween::LINEAR_OUT_SLOW_IN);

    gfx::Transform transform;
    if (should_show_text_filter) {
      transform.Translate(0, 0);
      text_filter_widget_window->layer()->SetOpacity(1);
    } else {
      transform.Translate(0, -text_filter_bottom_);
      text_filter_widget_window->layer()->SetOpacity(0);
    }

    text_filter_widget_window->SetTransform(transform);
    showing_text_filter_ = should_show_text_filter;
  }
  for (auto iter = grid_list_.begin(); iter != grid_list_.end(); iter++)
    (*iter)->FilterItems(new_contents);

  // If the selection widget is not active, execute a Move() command so that it
  // shows up on the first undimmed item.
  if (grid_list_[selected_grid_index_]->is_selecting())
    return;
  Move(WindowSelector::RIGHT, false);
}

void WindowSelector::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  if (state != SplitViewController::NO_SNAP) {
    // Do not restore focus if a window was just snapped and activated.
    ResetFocusRestoreWindow(false);
  }

  if (state == SplitViewController::BOTH_SNAPPED) {
    // If two windows were snapped to both sides of the screen, end overview
    // mode.
    CancelSelection();
  } else {
    // Otherwise adjust the overview window grid bounds if overview mode is
    // active at the moment.
    OnDisplayBoundsChanged();
  }
}

void WindowSelector::OnSplitViewDividerPositionChanged() {
  DCHECK(Shell::Get()->IsSplitViewModeActive());
  OnDisplayBoundsChanged();
}

aura::Window* WindowSelector::GetTextFilterWidgetWindow() {
  return text_filter_widget_->GetNativeWindow();
}

void WindowSelector::RepositionTextFilterOnDisplayMetricsChange() {
  const gfx::Rect rect = GetTextFilterPosition(Shell::GetPrimaryRootWindow());
  text_filter_bottom_ = rect.bottom() + kTextFieldBottomMargin;
  text_filter_widget_->SetBounds(rect);

  gfx::Transform transform;
  transform.Translate(
      0, text_filter_string_length_ == 0 ? -text_filter_bottom_ : 0);
  aura::Window* text_filter_window = GetTextFilterWidgetWindow();
  text_filter_window->layer()->SetOpacity(text_filter_string_length_ == 0 ? 0
                                                                          : 1);
  text_filter_window->SetTransform(transform);
}

void WindowSelector::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    wm::ActivateWindow(restore_focus_window_);
  }
  // If the window is in the observed_windows_ list it needs to continue to be
  // observed.
  if (observed_windows_.find(restore_focus_window_) ==
      observed_windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = nullptr;
}

void WindowSelector::Move(Direction direction, bool animate) {
  // Direction to move if moving past the end of a display.
  int display_direction = (direction == RIGHT || direction == DOWN) ? 1 : -1;

  // If this is the first move and it's going backwards, start on the last
  // display.
  if (display_direction == -1 && !grid_list_.empty() &&
      !grid_list_[selected_grid_index_]->is_selecting()) {
    selected_grid_index_ = grid_list_.size() - 1;
  }

  // Keep calling Move() on the grids until one of them reports no overflow or
  // we made a full cycle on all the grids.
  for (size_t i = 0; i <= grid_list_.size() &&
                     grid_list_[selected_grid_index_]->Move(direction, animate);
       i++) {
    selected_grid_index_ =
        (selected_grid_index_ + display_direction + grid_list_.size()) %
        grid_list_.size();
  }
}

void WindowSelector::OnDisplayBoundsChanged() {
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    SetBoundsForWindowGridsInScreen(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window())));
  }
  PositionWindows(/* animate */ false);
  RepositionTextFilterOnDisplayMetricsChange();
}

}  // namespace ash
