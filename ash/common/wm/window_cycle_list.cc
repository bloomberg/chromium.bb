// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/window_cycle_list.h"

#include <list>
#include <map>

#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/command_line.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/visibility_controller.h"

namespace ash {

namespace {

bool g_disable_initial_delay = false;

// Returns the window immediately below |window| in the current container.
WmWindow* GetWindowBelow(WmWindow* window) {
  WmWindow* parent = window->GetParent();
  if (!parent)
    return nullptr;
  const WmWindow::Windows children = parent->GetChildren();
  auto iter = std::find(children.begin(), children.end(), window);
  CHECK(*iter == window);
  return (iter != children.begin()) ? *(iter - 1) : nullptr;
}

// This background paints a |Painter| but fills the view's layer's size rather
// than the view's size.
class LayerFillBackgroundPainter : public views::Background {
 public:
  explicit LayerFillBackgroundPainter(std::unique_ptr<views::Painter> painter)
      : painter_(std::move(painter)) {}

  ~LayerFillBackgroundPainter() override {}

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    views::Painter::PaintPainterAt(canvas, painter_.get(),
                                   gfx::Rect(view->layer()->size()));
  }

 private:
  std::unique_ptr<views::Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(LayerFillBackgroundPainter);
};

}  // namespace

// This class restores and moves a window to the front of the stacking order for
// the duration of the class's scope.
class ScopedShowWindow : public WmWindowObserver {
 public:
  ScopedShowWindow();
  ~ScopedShowWindow() override;

  // Show |window| at the top of the stacking order.
  void Show(WmWindow* window);

  // Cancel restoring the window on going out of scope.
  void CancelRestore();

 private:
  // WmWindowObserver:
  void OnWindowTreeChanging(WmWindow* window,
                            const TreeChangeParams& params) override;

  // The window being shown.
  WmWindow* window_;

  // The window immediately below where window_ belongs.
  WmWindow* stack_window_above_;

  // If true, minimize window_ on going out of scope.
  bool minimized_;

  DISALLOW_COPY_AND_ASSIGN(ScopedShowWindow);
};

// This view represents a single WmWindow by displaying a title and a thumbnail
// of the window's contents.
class WindowPreviewView : public views::View, public WmWindowObserver {
 public:
  explicit WindowPreviewView(WmWindow* window)
      : window_title_(new views::Label),
        preview_background_(new views::View),
        mirror_view_(window->CreateViewWithRecreatedLayers().release()),
        window_observer_(this) {
    window_observer_.Add(window);
    window_title_->SetText(window->GetTitle());
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->SetEnabledColor(SK_ColorWHITE);
    window_title_->SetAutoColorReadabilityEnabled(false);
    // Background is not fully opaque, so subpixel rendering won't look good.
    window_title_->SetSubpixelRenderingEnabled(false);
    // The base font is 12pt (for English) so this comes out to 14pt.
    const int kLabelSizeDelta = 2;
    window_title_->SetFontList(
        window_title_->font_list().DeriveWithSizeDelta(kLabelSizeDelta));
    const int kAboveLabelPadding = 5;
    const int kBelowLabelPadding = 10;
    window_title_->SetBorder(views::Border::CreateEmptyBorder(
        kAboveLabelPadding, 0, kBelowLabelPadding, 0));
    AddChildView(window_title_);

    // Preview padding is black at 50% opacity.
    preview_background_->set_background(
        views::Background::CreateSolidBackground(
            SkColorSetA(SK_ColorBLACK, 0xFF / 2)));
    AddChildView(preview_background_);

    AddChildView(mirror_view_);

    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  ~WindowPreviewView() override {}

  // views::View:
  gfx::Size GetPreferredSize() const override {
    gfx::Size size = GetSizeForPreviewArea();
    size.Enlarge(0, window_title_->GetPreferredSize().height());
    return size;
  }

  void Layout() override {
    const gfx::Size preview_area_size = GetSizeForPreviewArea();
    // The window title is positioned above the preview area.
    window_title_->SetBounds(0, 0, width(),
                             height() - preview_area_size.height());

    gfx::Rect preview_area_bounds(preview_area_size);
    preview_area_bounds.set_y(height() - preview_area_size.height());
    mirror_view_->SetSize(GetMirrorViewScaledSize());
    if (mirror_view_->size() == preview_area_size) {
      // Padding is not needed, hide the background and set the mirror view
      // to take up the entire preview area.
      mirror_view_->SetPosition(preview_area_bounds.origin());
      preview_background_->SetVisible(false);
      return;
    }

    // Padding is needed, so show the background and set the mirror view to be
    // centered within it.
    preview_background_->SetBoundsRect(preview_area_bounds);
    preview_background_->SetVisible(true);
    preview_area_bounds.ClampToCenteredSize(mirror_view_->size());
    mirror_view_->SetPosition(preview_area_bounds.origin());
  }

  void GetAccessibleState(ui::AXViewState* state) override {
    state->role = ui::AX_ROLE_WINDOW;
    state->name = window_title_->text();
  }

  // WmWindowObserver:
  void OnWindowDestroying(WmWindow* window) override {
    window_observer_.Remove(window);
  }

  void OnWindowTitleChanged(WmWindow* window) override {
    window_title_->SetText(window->GetTitle());
  }

 private:
  // The maximum width of a window preview.
  static const int kMaxPreviewWidth = 512;
  // All previews are the same height (this is achieved via a combination of
  // scaling and padding).
  static const int kFixedPreviewHeight = 256;

  // Returns the size for the mirror view, scaled to fit within the max bounds.
  // Scaling is always 1:1 and we only scale down, never up.
  gfx::Size GetMirrorViewScaledSize() const {
    gfx::Size mirror_pref_size = mirror_view_->GetPreferredSize();

    if (mirror_pref_size.width() > kMaxPreviewWidth ||
        mirror_pref_size.height() > kFixedPreviewHeight) {
      float scale = std::min(
          kMaxPreviewWidth / static_cast<float>(mirror_pref_size.width()),
          kFixedPreviewHeight / static_cast<float>(mirror_pref_size.height()));
      mirror_pref_size =
          gfx::ScaleToFlooredSize(mirror_pref_size, scale, scale);
    }

    return mirror_pref_size;
  }

  // Returns the size for the entire preview area (mirror view and additional
  // padding). All previews will be the same height, so if the mirror view isn't
  // tall enough we will add top and bottom padding. Previews can range in width
  // from kMaxPreviewWidth down to half that value. Again, padding will be added
  // to the sides to achieve this if the preview is too narrow.
  gfx::Size GetSizeForPreviewArea() const {
    gfx::Size mirror_size = GetMirrorViewScaledSize();
    float aspect_ratio =
        static_cast<float>(mirror_size.width()) / mirror_size.height();
    gfx::Size preview_size = mirror_size;
    // Very narrow windows get vertical bars of padding on the sides.
    if (aspect_ratio < 0.5f)
      preview_size.set_width(mirror_size.height() / 2);

    // All previews are the same height (this may add padding on top and
    // bottom).
    preview_size.set_height(kFixedPreviewHeight);
    // Previews should never be narrower than half their max width (128dip).
    preview_size.set_width(
        std::max(preview_size.width(), kMaxPreviewWidth / 2));

    return preview_size;
  }

  // Displays the title of the window above the preview.
  views::Label* window_title_;
  // When visible, shows a darkened background area behind |mirror_view_|
  // (effectively padding the preview to fit the desired bounds).
  views::View* preview_background_;
  // The view that actually renders a thumbnail version of the window.
  views::View* mirror_view_;

  ScopedObserver<WmWindow, WmWindowObserver> window_observer_;

  DISALLOW_COPY_AND_ASSIGN(WindowPreviewView);
};

// A view that shows a collection of windows the user can tab through.
class WindowCycleView : public views::WidgetDelegateView {
 public:
  WindowCycleView(const WindowCycleList::WindowList& windows,
                  WindowCycleController::Direction initial_direction)
      : initial_direction_(initial_direction),
        mirror_container_(new views::View()),
        highlight_view_(new views::View()),
        target_window_(nullptr) {
    DCHECK(!windows.empty());
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetMasksToBounds(true);
    layer()->SetOpacity(0.0);
    {
      ui::ScopedLayerAnimationSettings animate_fade(layer()->GetAnimator());
      animate_fade.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(100));
      layer()->SetOpacity(1.0);
    }

    set_background(views::Background::CreateSolidBackground(
        SkColorSetA(SK_ColorBLACK, 0xE6)));

    const int kInsideBorderPaddingDip = 64;
    const int kBetweenChildPaddingDip = 10;
    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, kInsideBorderPaddingDip,
        kInsideBorderPaddingDip, kBetweenChildPaddingDip);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
    mirror_container_->SetLayoutManager(layout);
    mirror_container_->SetPaintToLayer(true);
    mirror_container_->layer()->SetFillsBoundsOpaquely(false);

    for (WmWindow* window : windows) {
      // |mirror_container_| owns |view|.
      views::View* view = new WindowPreviewView(window);
      window_view_map_[window] = view;
      mirror_container_->AddChildView(view);
    }

    const float kHighlightCornerRadius = 4;
    // The background needs to be painted to fill the layer, not the View,
    // because the layer animates bounds changes but the View's bounds change
    // immediately.
    highlight_view_->set_background(new LayerFillBackgroundPainter(
        base::WrapUnique(views::Painter::CreateRoundRectWith1PxBorderPainter(
            SkColorSetA(SK_ColorWHITE, 0x4D), SkColorSetA(SK_ColorWHITE, 0x33),
            kHighlightCornerRadius))));
    highlight_view_->SetPaintToLayer(true);
    highlight_view_->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(highlight_view_);
    AddChildView(mirror_container_);
  }

  ~WindowCycleView() override {}

  void SetTargetWindow(WmWindow* target) {
    target_window_ = target;
    if (GetWidget()) {
      Layout();
      if (target_window_) {
        // In the window destruction case, we may have already removed the
        // focused view and hence not be the focused window. We should still
        // always be active, though.
        DCHECK_EQ(ash::WmShell::Get()->GetActiveWindow()->GetInternalWidget(),
                  GetWidget());
        window_view_map_[target_window_]->RequestFocus();
      }
    }
  }

  void HandleWindowDestruction(WmWindow* destroying_window,
                               WmWindow* new_target) {
    auto view_iter = window_view_map_.find(destroying_window);
    views::View* preview = view_iter->second;
    views::View* parent = preview->parent();
    DCHECK_EQ(mirror_container_, parent);
    window_view_map_.erase(view_iter);
    delete preview;
    // With one of its children now gone, we must re-layout |mirror_container_|.
    // This must happen before SetTargetWindow() to make sure our own Layout()
    // works correctly when it's calculating highlight bounds.
    parent->Layout();
    SetTargetWindow(new_target);
  }

  // views::WidgetDelegateView overrides:
  gfx::Size GetPreferredSize() const override {
    return mirror_container_->GetPreferredSize();
  }

  void Layout() override {
    if (!target_window_ || bounds().IsEmpty())
      return;

    bool first_layout = mirror_container_->bounds().IsEmpty();
    // If |mirror_container_| has not yet been laid out, we must lay it and its
    // descendants out so that the calculations based on |target_view| work
    // properly.
    if (first_layout)
      mirror_container_->SizeToPreferredSize();

    // The preview list (|mirror_container_|) starts flush to the left of
    // the screen but moves to the left (off the edge of the screen) as the use
    // iterates over the previews. The list will move just enough to ensure the
    // highlighted preview is at or to the left of the center of the workspace.
    views::View* target_view = window_view_map_[target_window_];
    gfx::RectF target_bounds(target_view->GetLocalBounds());
    views::View::ConvertRectToTarget(target_view, mirror_container_,
                                     &target_bounds);
    gfx::Rect container_bounds(mirror_container_->GetPreferredSize());
    int x_offset =
        width() / 2 -
        mirror_container_->GetMirroredXInView(target_bounds.CenterPoint().x());
    if (initial_direction_ == WindowCycleController::FORWARD)
      x_offset = std::min(x_offset, 0);
    else
      x_offset = std::max(x_offset, width() - container_bounds.width());
    container_bounds.set_x(x_offset);
    mirror_container_->SetBoundsRect(container_bounds);

    // Calculate the target preview's bounds relative to |this|.
    views::View::ConvertRectToTarget(mirror_container_, this, &target_bounds);
    const int kHighlightPaddingDip = 5;
    target_bounds.Inset(gfx::InsetsF(-kHighlightPaddingDip));
    target_bounds.set_x(
        GetMirroredXWithWidthInView(target_bounds.x(), target_bounds.width()));
    highlight_view_->SetBoundsRect(gfx::ToEnclosingRect(target_bounds));

    // Enable animations only after the first Layout() pass.
    if (first_layout) {
      // The preview list animates bounds changes (other animatable properties
      // never change).
      mirror_container_->layer()->SetAnimator(
          ui::LayerAnimator::CreateImplicitAnimator());
      // The selection highlight also animates all bounds changes and never
      // changes other animatable properties.
      highlight_view_->layer()->SetAnimator(
          ui::LayerAnimator::CreateImplicitAnimator());
    }
  }

  void OnMouseCaptureLost() override {
    WmShell::Get()->window_cycle_controller()->StopCycling();
  }

  View* GetInitiallyFocusedView() override {
    return window_view_map_[target_window_];
  }

  WmWindow* target_window() { return target_window_; }

 private:
  WindowCycleController::Direction initial_direction_;
  std::map<WmWindow*, views::View*> window_view_map_;
  views::View* mirror_container_;
  views::View* highlight_view_;
  WmWindow* target_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleView);
};

ScopedShowWindow::ScopedShowWindow()
    : window_(nullptr), stack_window_above_(nullptr), minimized_(false) {}

ScopedShowWindow::~ScopedShowWindow() {
  if (window_) {
    window_->GetParent()->RemoveObserver(this);

    // Restore window's stacking position.
    if (stack_window_above_)
      window_->GetParent()->StackChildAbove(window_, stack_window_above_);
    else
      window_->GetParent()->StackChildAtBottom(window_);

    // Restore minimized state.
    if (minimized_)
      window_->GetWindowState()->Minimize();
  }
}

void ScopedShowWindow::Show(WmWindow* window) {
  DCHECK(!window_);
  window_ = window;
  stack_window_above_ = GetWindowBelow(window);
  minimized_ = window->GetWindowState()->IsMinimized();
  window_->GetParent()->AddObserver(this);
  window_->Show();
  window_->GetWindowState()->Activate();
}

void ScopedShowWindow::CancelRestore() {
  if (!window_)
    return;
  window_->GetParent()->RemoveObserver(this);
  window_ = stack_window_above_ = nullptr;
}

void ScopedShowWindow::OnWindowTreeChanging(WmWindow* window,
                                            const TreeChangeParams& params) {
  // Only interested in removal.
  if (params.new_parent != nullptr)
    return;

  if (params.target == window_) {
    CancelRestore();
  } else if (params.target == stack_window_above_) {
    // If the window this window was above is removed, use the next window down
    // as the restore marker.
    stack_window_above_ = GetWindowBelow(stack_window_above_);
  }
}

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows),
      current_index_(0),
      initial_direction_(WindowCycleController::FORWARD),
      cycle_view_(nullptr),
      cycle_ui_widget_(nullptr),
      screen_observer_(this) {
  if (!ShouldShowUi())
    WmShell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  for (WmWindow* window : windows_)
    window->AddObserver(this);

  if (ShouldShowUi()) {
    if (g_disable_initial_delay) {
      InitWindowCycleView();
    } else {
      show_ui_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(150),
                           this, &WindowCycleList::InitWindowCycleView);
    }
  }
}

WindowCycleList::~WindowCycleList() {
  if (!ShouldShowUi())
    WmShell::Get()->mru_window_tracker()->SetIgnoreActivations(false);

  for (WmWindow* window : windows_)
    window->RemoveObserver(this);

  if (showing_window_) {
    showing_window_->CancelRestore();
  } else if (!windows_.empty()) {
    WmWindow* target_window = windows_[current_index_];
    target_window->Show();
    target_window->GetWindowState()->Activate();
  }

  if (cycle_ui_widget_)
    cycle_ui_widget_->Close();
}

void WindowCycleList::Step(WindowCycleController::Direction direction) {
  if (windows_.empty())
    return;

  // When there is only one window, we should give feedback to the user. If the
  // window is minimized, we should also show it.
  if (windows_.size() == 1) {
    windows_[0]->Animate(::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    windows_[0]->Show();
    windows_[0]->GetWindowState()->Activate();
    return;
  }

  DCHECK(static_cast<size_t>(current_index_) < windows_.size());

  if (!cycle_view_ && current_index_ == 0) {
    initial_direction_ = direction;
    // Special case the situation where we're cycling forward but the MRU window
    // is not active. This occurs when all windows are minimized. The starting
    // window should be the first one rather than the second.
    if (direction == WindowCycleController::FORWARD && !windows_[0]->IsActive())
      current_index_ = -1;
  }

  // We're in a valid cycle, so step forward or backward.
  current_index_ += direction == WindowCycleController::FORWARD ? 1 : -1;

  // Wrap to window list size.
  current_index_ = (current_index_ + windows_.size()) % windows_.size();
  DCHECK(windows_[current_index_]);

  if (ShouldShowUi()) {
    if (current_index_ > 1)
      InitWindowCycleView();

    if (cycle_view_)
      cycle_view_->SetTargetWindow(windows_[current_index_]);
  } else {
    // Make sure the next window is visible.
    showing_window_.reset(new ScopedShowWindow);
    showing_window_->Show(windows_[current_index_]);
  }
}

// static
void WindowCycleList::DisableInitialDelayForTesting() {
  g_disable_initial_delay = true;
}

void WindowCycleList::OnWindowDestroying(WmWindow* window) {
  window->RemoveObserver(this);

  WindowList::iterator i = std::find(windows_.begin(), windows_.end(), window);
  // TODO(oshima): Change this back to DCHECK once crbug.com/483491 is fixed.
  CHECK(i != windows_.end());
  int removed_index = static_cast<int>(i - windows_.begin());
  windows_.erase(i);
  if (current_index_ > removed_index ||
      current_index_ == static_cast<int>(windows_.size())) {
    current_index_--;
  }

  if (cycle_view_) {
    WmWindow* new_target_window =
        windows_.empty() ? nullptr : windows_[current_index_];
    cycle_view_->HandleWindowDestruction(window, new_target_window);
    if (windows_.empty()) {
      // This deletes us.
      WmShell::Get()->window_cycle_controller()->StopCycling();
      return;
    }
  }
}

void WindowCycleList::OnDisplayAdded(const display::Display& new_display) {}

void WindowCycleList::OnDisplayRemoved(const display::Display& old_display) {}

void WindowCycleList::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t changed_metrics) {
  if (cycle_ui_widget_ &&
      display.id() ==
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(cycle_ui_widget_->GetNativeView())
              .id() &&
      (changed_metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION))) {
    WmShell::Get()->window_cycle_controller()->StopCycling();
    // |this| is deleted.
    return;
  }
}

bool WindowCycleList::ShouldShowUi() {
  return windows_.size() > 1;
}

void WindowCycleList::InitWindowCycleView() {
  if (cycle_view_)
    return;

  cycle_view_ = new WindowCycleView(windows_, initial_direction_);
  cycle_view_->SetTargetWindow(windows_[current_index_]);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.delegate = cycle_view_;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.name = "WindowCycleList (Alt+Tab)";
  // TODO(estade): make sure nothing untoward happens when the lock screen
  // or a system modal dialog is shown.
  WmWindow* root_window = WmShell::Get()->GetRootWindowForNewWindows();
  root_window->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
      widget, kShellWindowId_OverlayContainer, &params);
  gfx::Rect widget_rect = root_window->GetDisplayNearestWindow().bounds();
  int widget_height = cycle_view_->GetPreferredSize().height();
  widget_rect.set_y((widget_rect.height() - widget_height) / 2);
  widget_rect.set_height(widget_height);
  params.bounds = widget_rect;
  widget->Init(params);

  screen_observer_.Add(display::Screen::GetScreen());
  widget->Show();
  widget->SetCapture(cycle_view_);
  widget->set_auto_release_capture(false);
  cycle_ui_widget_ = widget;
}

}  // namespace ash
