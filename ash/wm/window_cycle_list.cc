// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_list.h"

#include <list>
#include <map>

#include "ash/common/ash_switches.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/forwarding_layer_delegate.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

void EnsureAllChildrenAreVisible(ui::Layer* layer) {
  std::list<ui::Layer*> layers;
  layers.push_back(layer);
  while (!layers.empty()) {
    for (auto child : layers.front()->children())
      layers.push_back(child);
    layers.front()->SetVisible(true);
    layers.pop_front();
  }
}

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

// A view that mirrors a single window. Layers are lifted from the underlying
// window (which gets new ones in their place). New paint calls, if any, are
// forwarded to the underlying window.
class WindowMirrorView : public views::View, public ::wm::LayerDelegateFactory {
 public:
  explicit WindowMirrorView(WmWindow* window) : target_(window) {
    DCHECK(window);
  }
  ~WindowMirrorView() override {}

  void Init() {
    SetPaintToLayer(true);

    layer_owner_ = ::wm::RecreateLayers(
        target_->GetInternalWidget()->GetNativeView(), this);

    GetMirrorLayer()->parent()->Remove(GetMirrorLayer());
    layer()->Add(GetMirrorLayer());

    // Some extra work is needed when the target window is minimized.
    if (target_->GetWindowState()->IsMinimized()) {
      GetMirrorLayer()->SetVisible(true);
      GetMirrorLayer()->SetOpacity(1);
      EnsureAllChildrenAreVisible(GetMirrorLayer());
    }
  }

  // views::View:
  gfx::Size GetPreferredSize() const override {
    const int kMaxWidth = 800;
    const int kMaxHeight = 600;

    gfx::Size target_size = target_->GetBounds().size();
    if (target_size.width() <= kMaxWidth &&
        target_size.height() <= kMaxHeight) {
      return target_size;
    }

    float scale =
        std::min(kMaxWidth / static_cast<float>(target_size.width()),
                 kMaxHeight / static_cast<float>(target_size.height()));
    return gfx::ScaleToCeiledSize(target_size, scale, scale);
  }

  void Layout() override {
    // Position at 0, 0.
    GetMirrorLayer()->SetBounds(gfx::Rect(GetMirrorLayer()->bounds().size()));

    // Scale down if necessary.
    gfx::Transform mirror_transform;
    if (size() != target_->GetBounds().size()) {
      const float scale =
          width() / static_cast<float>(target_->GetBounds().width());
      mirror_transform.Scale(scale, scale);
    }
    GetMirrorLayer()->SetTransform(mirror_transform);
  }

  // ::wm::LayerDelegateFactory:
  ui::LayerDelegate* CreateDelegate(ui::LayerDelegate* delegate) override {
    if (!delegate)
      return nullptr;
    delegates_.push_back(
        base::WrapUnique(new wm::ForwardingLayerDelegate(target_, delegate)));

    return delegates_.back().get();
  }

 private:
  // Gets the root of the layer tree that was lifted from |target_| (and is now
  // a child of |this->layer()|).
  ui::Layer* GetMirrorLayer() { return layer_owner_->root(); }

  // The original window that is being represented by |this|.
  WmWindow* target_;

  // Retains ownership of the mirror layer tree.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  std::vector<std::unique_ptr<wm::ForwardingLayerDelegate>> delegates_;

  DISALLOW_COPY_AND_ASSIGN(WindowMirrorView);
};

// A view that shows a collection of windows the user can tab through.
class WindowCycleView : public views::View {
 public:
  explicit WindowCycleView(const WindowCycleList::WindowList& windows)
      : mirror_container_(new views::View()),
        selector_view_(new views::View()),
        target_window_(nullptr) {
    DCHECK(!windows.empty());
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);

    // TODO(estade): adjust constants in this function (colors, spacing, corner
    // radius) as per mocks.
    const float kCornerRadius = 5;
    set_background(views::Background::CreateBackgroundPainter(
        true, views::Painter::CreateSolidRoundRectPainter(
                  SkColorSetA(SK_ColorBLACK, 0xA5), kCornerRadius)));

    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 25, 25, 20);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
    mirror_container_->SetLayoutManager(layout);

    for (WmWindow* window : windows) {
      WindowMirrorView* view = new WindowMirrorView(window);
      view->Init();
      window_view_map_[window] = view;
      mirror_container_->AddChildView(view);
    }

    selector_view_->set_background(views::Background::CreateBackgroundPainter(
        true, views::Painter::CreateSolidRoundRectPainter(SK_ColorBLUE,
                                                          kCornerRadius)));

    AddChildView(selector_view_);
    AddChildView(mirror_container_);
    SetTargetWindow(windows.front());
  }

  ~WindowCycleView() override {}

  void SetTargetWindow(WmWindow* target) {
    target_window_ = target;
    if (GetWidget())
      Layout();
  }

  void HandleWindowDestruction(WmWindow* destroying_window,
                               WmWindow* new_target) {
    auto view_iter = window_view_map_.find(destroying_window);
    view_iter->second->parent()->RemoveChildView(view_iter->second);
    window_view_map_.erase(view_iter);
    SetTargetWindow(new_target);
  }

  // views::View overrides:
  gfx::Size GetPreferredSize() const override {
    return mirror_container_->GetPreferredSize();
  }

  void Layout() override {
    // Possible if the last window is deleted.
    if (!target_window_)
      return;

    views::View* target_view = window_view_map_[target_window_];
    gfx::RectF target_bounds(target_view->GetLocalBounds());
    views::View::ConvertRectToTarget(target_view, this, &target_bounds);
    target_bounds.Inset(gfx::InsetsF(-15));
    selector_view_->SetBoundsRect(gfx::ToEnclosingRect(target_bounds));

    mirror_container_->SetBoundsRect(GetLocalBounds());
  }

  WmWindow* target_window() { return target_window_; }

 private:
  std::map<WmWindow*, WindowMirrorView*> window_view_map_;
  views::View* mirror_container_;
  views::View* selector_view_;
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
    : windows_(windows), current_index_(0), cycle_view_(nullptr) {
  WmShell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  for (WmWindow* window : windows_)
    window->AddObserver(this);

  if (ShouldShowUi()) {
    WmWindow* root_window = WmShell::Get()->GetRootWindowForNewWindows();
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.accept_events = true;
    // TODO(estade): make sure nothing untoward happens when the lock screen
    // or a system modal dialog is shown.
    root_window->GetRootWindowController()
        ->ConfigureWidgetInitParamsForContainer(
            widget, kShellWindowId_OverlayContainer, &params);
    widget->Init(params);

    cycle_view_ = new WindowCycleView(windows_);

    widget->SetContentsView(cycle_view_);
    // TODO(estade): right now this just extends past the edge of the screen if
    // there are too many windows. Handle this more gracefully. Also, if
    // the display metrics change, cancel the UI.
    gfx::Rect widget_rect = widget->GetWorkAreaBoundsInScreen();
    gfx::Size widget_size = cycle_view_->GetPreferredSize();
    widget_rect.ClampToCenteredSize(widget_size);
    widget_rect.set_width(widget_size.width());
    widget->SetBounds(widget_rect);
    widget->Show();
    cycle_ui_widget_.reset(widget);
  }
}

WindowCycleList::~WindowCycleList() {
  WmShell::Get()->mru_window_tracker()->SetIgnoreActivations(false);
  for (WmWindow* window : windows_)
    window->RemoveObserver(this);

  if (showing_window_)
    showing_window_->CancelRestore();

  if (cycle_view_ && cycle_view_->target_window()) {
    cycle_view_->target_window()->Show();
    cycle_view_->target_window()->GetWindowState()->Activate();
  }
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

  // We're in a valid cycle, so step forward or backward.
  current_index_ += direction == WindowCycleController::FORWARD ? 1 : -1;

  // Wrap to window list size.
  current_index_ = (current_index_ + windows_.size()) % windows_.size();
  DCHECK(windows_[current_index_]);

  if (cycle_view_) {
    cycle_view_->SetTargetWindow(windows_[current_index_]);
    return;
  }

  // Make sure the next window is visible.
  showing_window_.reset(new ScopedShowWindow);
  showing_window_->Show(windows_[current_index_]);
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
      Shell::GetInstance()->window_cycle_controller()->StopCycling();
      return;
    }
  }
}

bool WindowCycleList::ShouldShowUi() {
  return windows_.size() > 1 &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshEnableWindowCycleUi);
}

}  // namespace ash
