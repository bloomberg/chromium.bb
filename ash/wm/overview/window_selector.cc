// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>
#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/switchable_windows.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_user_metrics_action.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_item.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The proportion of screen width that the text filter takes.
const float kTextFilterScreenProportion = 0.25;

// The amount of padding surrounding the text in the text filtering textbox.
const int kTextFilterHorizontalPadding = 8;

// The distance between the top of the screen and the top edge of the
// text filtering textbox.
const int kTextFilterDistanceFromTop = 32;

// The height of the text filtering textbox.
const int kTextFilterHeight = 32;

// The font style used for text filtering.
static const ::ui::ResourceBundle::FontStyle kTextFilterFontStyle =
    ::ui::ResourceBundle::FontStyle::MediumFont;

// The alpha value for the background of the text filtering textbox.
const unsigned char kTextFilterOpacity = 180;

// The radius used for the rounded corners on the text filtering textbox.
const int kTextFilterCornerRadius = 1;

// A comparator for locating a grid with a given root window.
struct RootWindowGridComparator {
  explicit RootWindowGridComparator(const WmWindow* root_window)
      : root_window_(root_window) {}

  bool operator()(const std::unique_ptr<WindowGrid>& grid) const {
    return grid->root_window() == root_window_;
  }

  const WmWindow* root_window_;
};

// A comparator for locating a selectable window given a targeted window.
struct WindowSelectorItemTargetComparator {
  explicit WindowSelectorItemTargetComparator(const WmWindow* target_window)
      : target(target_window) {}

  bool operator()(WindowSelectorItem* window) const {
    return window->GetWindow() == target;
  }

  const WmWindow* target;
};

// A comparator for locating a selector item for a given root.
struct WindowSelectorItemForRoot {
  explicit WindowSelectorItemForRoot(const WmWindow* root)
      : root_window(root) {}

  bool operator()(WindowSelectorItem* item) const {
    return item->root_window() == root_window;
  }

  const WmWindow* root_window;
};

// A View having rounded corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
// TODO(tdanderson): This duplicates code from RoundedImageView. Refactor these
//                   classes and move into ui/views.
class RoundedContainerView : public views::View {
 public:
  RoundedContainerView(int corner_radius, SkColor background)
      : corner_radius_(corner_radius),
        background_(background) {
  }

  ~RoundedContainerView() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    SkScalar radius = SkIntToScalar(corner_radius_);
    const SkScalar kRadius[8] = {radius, radius, radius, radius,
                                 radius, radius, radius, radius};
    SkPath path;
    gfx::Rect bounds(size());
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

    SkPaint paint;
    paint.setAntiAlias(true);
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
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows())
    root->GetRootWindowController()->GetShelf()->UpdateVisibilityState();
}

// Initializes the text filter on the top of the main root window and requests
// focus on its textfield.
views::Widget* CreateTextFilter(views::TextfieldController* controller,
                                WmWindow* root_window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.bounds = gfx::Rect(
      root_window->GetBounds().width() / 2 * (1 - kTextFilterScreenProportion),
      kTextFilterDistanceFromTop,
      root_window->GetBounds().width() * kTextFilterScreenProportion,
      kTextFilterHeight);
  root_window->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
      widget, kShellWindowId_OverlayContainer, &params);
  widget->Init(params);

  // Use |container| to specify the padding surrounding the text and to give
  // the textfield rounded corners.
  views::View* container = new RoundedContainerView(
      kTextFilterCornerRadius, SkColorSetARGB(kTextFilterOpacity, 0, 0, 0));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  int text_height = bundle.GetFontList(kTextFilterFontStyle).GetHeight();
  DCHECK(text_height);
  int vertical_padding = (kTextFilterHeight - text_height) / 2;
  container->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                                   kTextFilterHorizontalPadding,
                                                   vertical_padding,
                                                   0));

  views::Textfield* textfield = new views::Textfield;
  textfield->set_controller(controller);
  textfield->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield->SetBorder(views::Border::NullBorder());
  textfield->SetTextColor(SK_ColorWHITE);
  textfield->SetFontList(bundle.GetFontList(kTextFilterFontStyle));

  container->AddChildView(textfield);
  widget->SetContentsView(container);

  // The textfield initially contains no text, so shift its position to be
  // outside the visible bounds of the screen.
  gfx::Transform transform;
  transform.Translate(0, -WindowSelector::kTextFilterBottomEdge);
  WmLookup::Get()->GetWindowForWidget(widget)->SetTransform(transform);
  widget->Show();
  textfield->RequestFocus();

  return widget;
}

}  // namespace

const int WindowSelector::kTextFilterBottomEdge =
    kTextFilterDistanceFromTop + kTextFilterHeight;

// static
bool WindowSelector::IsSelectable(WmWindow* window) {
  wm::WindowState* state = window->GetWindowState();
  if (state->GetStateType() == wm::WINDOW_STATE_TYPE_DOCKED ||
      state->GetStateType() == wm::WINDOW_STATE_TYPE_DOCKED_MINIMIZED) {
    return false;
  }
  return state->IsUserPositionable();
}

WindowSelector::WindowSelector(WindowSelectorDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(WmShell::Get()->GetFocusedWindow()),
      ignore_activations_(false),
      selected_grid_index_(0),
      overview_start_time_(base::Time::Now()),
      num_key_presses_(0),
      num_items_(0),
      showing_selection_widget_(false),
      text_filter_string_length_(0),
      num_times_textfield_cleared_(0),
      restoring_minimized_windows_(false) {
  DCHECK(delegate_);
}

WindowSelector::~WindowSelector() {
  RemoveAllObservers();
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, (ie PanelLayoutManager) to make indirect
// calls to restoring_minimized_windows() on a partially constructed object.
void WindowSelector::Init(const WindowList& windows) {
  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  WmShell* shell = WmShell::Get();

  std::vector<WmWindow*> root_windows = shell->GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const WmWindow* a, const WmWindow* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (WmWindow* root : root_windows) {
    // Observed switchable containers for newly created windows on all root
    // windows.
    for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
      WmWindow* container =
          root->GetChildByShellWindowId(wm::kSwitchableWindowContainerIds[i]);
      container->AddObserver(this);
      observed_windows_.insert(container);
    }

    // Hide the callout widgets for panels. It is safe to call this for
    // root windows that don't contain any panel windows.
    PanelLayoutManager::Get(root)->SetShowCalloutWidgets(false);

    std::unique_ptr<WindowGrid> grid(new WindowGrid(root, windows, this));
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

    text_filter_widget_.reset(
        CreateTextFilter(this, shell->GetPrimaryRootWindow()));
  }

  DCHECK(!grid_list_.empty());
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", num_items_);

  shell->AddActivationObserver(this);

  display::Screen::GetScreen()->AddObserver(this);
  shell->RecordUserMetricsAction(wm::WmUserMetricsAction::WINDOW_OVERVIEW);
  // Send an a11y alert.
  Shell::GetInstance()->accessibility_delegate()->TriggerAccessibilityAlert(
      ui::A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);

  UpdateShelfVisibility();
}

// NOTE: The work done in Shutdown() is not done in the destructor because it
// may cause other, unrelated classes, (ie PanelLayoutManager) to make indirect
// calls to restoring_minimized_windows() on a partially destructed object.
void WindowSelector::Shutdown() {
  ResetFocusRestoreWindow(true);
  RemoveAllObservers();

  std::vector<WmWindow*> root_windows = WmShell::Get()->GetAllRootWindows();
  for (WmWindow* window : root_windows) {
    // Un-hide the callout widgets for panels. It is safe to call this for
    // root_windows that don't contain any panel windows.
    PanelLayoutManager::Get(window)->SetShowCalloutWidgets(true);
  }

  size_t remaining_items = 0;
  for (std::unique_ptr<WindowGrid>& window_grid : grid_list_) {
    for (WindowSelectorItem* window_selector_item : window_grid->window_list())
      window_selector_item->RestoreWindow();
    remaining_items += window_grid->size();
  }

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
    UMA_HISTOGRAM_COUNTS_100(
        "Ash.WindowSelector.ItemsWhenTextFilteringUsed",
        remaining_items);
  }

  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  grid_list_.clear();
  UpdateShelfVisibility();
}

void WindowSelector::RemoveAllObservers() {
  for (WmWindow* window : observed_windows_)
    window->RemoveObserver(this);

  WmShell::Get()->RemoveActivationObserver(this);
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

void WindowSelector::SelectWindow(WmWindow* window) {
  // Record UMA_WINDOW_OVERVIEW_ACTIVE_WINDOW_CHANGED if the user is selecting
  // a window other than the window that was active prior to entering overview
  // mode (i.e., the window at the front of the MRU list).
  std::vector<WmWindow*> window_list = WmShell::Get()->GetMruWindowList();
  if (!window_list.empty() && window_list[0] != window) {
    WmShell::Get()->RecordUserMetricsAction(
        wm::WmUserMetricsAction::WINDOW_OVERVIEW_ACTIVE_WINDOW_CHANGED);
  }

  window->GetWindowState()->Activate();
}

bool WindowSelector::HandleKeyEvent(views::Textfield* sender,
                                    const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  switch (key_event.key_code()) {
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
      num_key_presses_++;
      Move(WindowSelector::RIGHT, true);
      break;
    case ui::VKEY_LEFT:
      num_key_presses_++;
      Move(WindowSelector::LEFT, true);
      break;
    case ui::VKEY_RETURN:
      // Ignore if no item is selected.
      if (!grid_list_[selected_grid_index_]->is_selecting())
        return false;
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ArrowKeyPresses",
                               num_key_presses_);
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Ash.WindowSelector.KeyPressesOverItemsRatio",
          (num_key_presses_ * 100) / num_items_, 1, 300, 30);
      WmShell::Get()->RecordUserMetricsAction(
          wm::WmUserMetricsAction::WINDOW_OVERVIEW_ENTER_KEY);
      SelectWindow(
          grid_list_[selected_grid_index_]->SelectedWindow()->GetWindow());
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
  PositionWindows(/* animate */ false);
  RepositionTextFilterOnDisplayMetricsChange();
}

void WindowSelector::OnWindowTreeChanged(WmWindow* window,
                                         const TreeChangeParams& params) {
  // Only care about newly added children of |observed_windows_|.
  if (!observed_windows_.count(window) ||
      !observed_windows_.count(params.new_parent)) {
    return;
  }

  WmWindow* new_window = params.target;
  if (!IsSelectable(new_window))
    return;

  for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
    if (new_window->GetParent()->GetShellWindowId() ==
            wm::kSwitchableWindowContainerIds[i] &&
        !new_window->GetTransientParent()) {
      // The new window is in one of the switchable containers, abort overview.
      CancelSelection();
      return;
    }
  }
}

void WindowSelector::OnWindowDestroying(WmWindow* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = nullptr;
}

void WindowSelector::OnWindowActivated(WmWindow* gained_active,
                                       WmWindow* lost_active) {
  if (ignore_activations_ || !gained_active ||
      gained_active == GetTextFilterWidgetWindow()) {
    return;
  }

  auto grid =
      std::find_if(grid_list_.begin(), grid_list_.end(),
                   RootWindowGridComparator(gained_active->GetRootWindow()));
  if (grid == grid_list_.end())
    return;
  const std::vector<WindowSelectorItem*> windows = (*grid)->window_list();

  auto iter = std::find_if(windows.begin(), windows.end(),
                           WindowSelectorItemTargetComparator(gained_active));

  if (iter != windows.end())
    (*iter)->ShowWindowOnExit();

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  CancelSelection();
}

void WindowSelector::OnAttemptToReactivateWindow(WmWindow* request_active,
                                                 WmWindow* actual_active) {
  OnWindowActivated(request_active, actual_active);
}

void WindowSelector::ContentsChanged(views::Textfield* sender,
                                     const base::string16& new_contents) {
  text_filter_string_length_ = new_contents.length();
  if (!text_filter_string_length_)
    num_times_textfield_cleared_++;

  bool should_show_selection_widget = !new_contents.empty();
  if (showing_selection_widget_ != should_show_selection_widget) {
    WmWindow* text_filter_widget_window = GetTextFilterWidgetWindow();
    ui::ScopedLayerAnimationSettings animation_settings(
        text_filter_widget_window->GetLayer()->GetAnimator());
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTweenType(showing_selection_widget_ ?
        gfx::Tween::FAST_OUT_LINEAR_IN : gfx::Tween::LINEAR_OUT_SLOW_IN);

    gfx::Transform transform;
    if (should_show_selection_widget) {
      transform.Translate(0, 0);
      text_filter_widget_window->SetOpacity(1);
    } else {
      transform.Translate(0, -kTextFilterBottomEdge);
      text_filter_widget_window->SetOpacity(0);
    }

    text_filter_widget_window->SetTransform(transform);
    showing_selection_widget_ = should_show_selection_widget;
  }
  for (auto iter = grid_list_.begin(); iter != grid_list_.end(); iter++)
    (*iter)->FilterItems(new_contents);

  // If the selection widget is not active, execute a Move() command so that it
  // shows up on the first undimmed item.
  if (grid_list_[selected_grid_index_]->is_selecting())
    return;
  Move(WindowSelector::RIGHT, false);
}

WmWindow* WindowSelector::GetTextFilterWidgetWindow() {
  return WmLookup::Get()->GetWindowForWidget(text_filter_widget_.get());
}

void WindowSelector::PositionWindows(bool animate) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->PositionWindows(animate);
}

void WindowSelector::RepositionTextFilterOnDisplayMetricsChange() {
  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  gfx::Rect rect(
      root_window->GetBounds().width() / 2 * (1 - kTextFilterScreenProportion),
      kTextFilterDistanceFromTop,
      root_window->GetBounds().width() * kTextFilterScreenProportion,
      kTextFilterHeight);

  text_filter_widget_->SetBounds(rect);

  gfx::Transform transform;
  transform.Translate(0, text_filter_string_length_ == 0
                             ? -WindowSelector::kTextFilterBottomEdge
                             : 0);
  GetTextFilterWidgetWindow()->SetTransform(transform);
}

void WindowSelector::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    restore_focus_window_->Activate();
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
  for (size_t i = 0;
      i <= grid_list_.size() &&
      grid_list_[selected_grid_index_]->Move(direction, animate); i++) {
    selected_grid_index_ =
        (selected_grid_index_ + display_direction + grid_list_.size()) %
        grid_list_.size();
  }
}

}  // namespace ash
