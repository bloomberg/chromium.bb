// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/panel_stack_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/views/panels/panel_view.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/panels/taskbar_window_thumbnailer_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {
// These values are experimental and subjective.
const int kDefaultFramerateHz = 50;
const int kSetBoundsAnimationMs = 180;
}

// static
NativePanelStackWindow* NativePanelStackWindow::Create(
    NativePanelStackWindowDelegate* delegate) {
#if defined(OS_WIN)
  return new PanelStackView(delegate);
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

PanelStackView::PanelStackView(NativePanelStackWindowDelegate* delegate)
    : delegate_(delegate),
      window_(NULL),
      is_drawing_attention_(false),
      animate_bounds_updates_(false),
      bounds_updates_started_(false) {
  DCHECK(delegate);
}

PanelStackView::~PanelStackView() {
}

void PanelStackView::Close() {
  delegate_ = NULL;
  if (bounds_animator_)
    bounds_animator_.reset();
  if (window_)
    window_->Close();
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void PanelStackView::AddPanel(Panel* panel) {
  panels_.push_back(panel);

  EnsureWindowCreated();
  MakeStackWindowOwnPanelWindow(panel, this);
  UpdateStackWindowBounds();

  window_->UpdateWindowTitle();
  window_->UpdateWindowIcon();
}

void PanelStackView::RemovePanel(Panel* panel) {
  panels_.remove(panel);

  MakeStackWindowOwnPanelWindow(panel, NULL);
  UpdateStackWindowBounds();
}

void PanelStackView::MergeWith(NativePanelStackWindow* another) {
  PanelStackView* another_stack = static_cast<PanelStackView*>(another);

  for (Panels::const_iterator iter = another_stack->panels_.begin();
       iter != another_stack->panels_.end(); ++iter) {
    Panel* panel = *iter;
    panels_.push_back(panel);
    MakeStackWindowOwnPanelWindow(panel, this);
  }
  another_stack->panels_.clear();

  UpdateStackWindowBounds();
}

bool PanelStackView::IsEmpty() const {
  return panels_.empty();
}

bool PanelStackView::HasPanel(Panel* panel) const {
  return std::find(panels_.begin(), panels_.end(), panel) != panels_.end();
}

void PanelStackView::MovePanelsBy(const gfx::Vector2d& delta) {
  BeginBatchUpdatePanelBounds(false);
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    AddPanelBoundsForBatchUpdate(panel, panel->GetBounds() + delta);
  }
  EndBatchUpdatePanelBounds();
}

void PanelStackView::BeginBatchUpdatePanelBounds(bool animate) {
  // If the batch animation is still in progress, continue the animation
  // with the new target bounds even we want to update the bounds instantly
  // this time.
  if (!bounds_updates_started_) {
    animate_bounds_updates_ = animate;
    bounds_updates_started_ = true;
  }
}

void PanelStackView::AddPanelBoundsForBatchUpdate(Panel* panel,
                                                  const gfx::Rect& new_bounds) {
  DCHECK(bounds_updates_started_);

  // No need to track it if no change is needed.
  if (panel->GetBounds() == new_bounds)
    return;

  // Old bounds are stored as the map value.
  bounds_updates_[panel] = panel->GetBounds();

  // New bounds are directly applied to the valued stored in native panel
  // window.
  static_cast<PanelView*>(panel->native_panel())->set_cached_bounds_directly(
      new_bounds);
}

void PanelStackView::EndBatchUpdatePanelBounds() {
  DCHECK(bounds_updates_started_);

  if (bounds_updates_.empty() || !animate_bounds_updates_) {
    if (!bounds_updates_.empty()) {
      UpdatePanelsBounds();
      bounds_updates_.clear();
    }

    bounds_updates_started_ = false;
    delegate_->PanelBoundsBatchUpdateCompleted();
    return;
  }

  bounds_animator_.reset(new ui::LinearAnimation(
      PanelManager::AdjustTimeInterval(kSetBoundsAnimationMs),
      kDefaultFramerateHz,
      this));
  bounds_animator_->Start();
}

bool PanelStackView::IsAnimatingPanelBounds() const {
  return bounds_updates_started_ && animate_bounds_updates_;
}

void PanelStackView::Minimize() {
#if defined(OS_WIN)
  // When the owner stack window is minimized by the system, its live preview
  // is lost. We need to set it explicitly. This has to be done before the
  // minimization.
  CaptureThumbnailForLivePreview();
#endif

  window_->Minimize();
}

bool PanelStackView::IsMinimized() const {
  return window_ ? window_->IsMinimized() : false;
}

void PanelStackView::DrawSystemAttention(bool draw_attention) {
  // The underlying call of FlashFrame, FlashWindowEx, seems not to work
  // correctly if it is called more than once consecutively.
  if (draw_attention == is_drawing_attention_)
    return;
  is_drawing_attention_ = draw_attention;

  window_->FlashFrame(draw_attention);
}

string16 PanelStackView::GetWindowTitle() const {
  return delegate_->GetTitle();
}

gfx::ImageSkia PanelStackView::GetWindowAppIcon() {
  if (panels_.empty())
    return gfx::ImageSkia();

  Panel* panel = panels_.front();
  gfx::Image app_icon = panel->app_icon();
  if (!app_icon.IsEmpty())
    return *app_icon.ToImageSkia();

  return gfx::ImageSkia();
}

gfx::ImageSkia PanelStackView::GetWindowIcon() {
  return GetWindowAppIcon();
}

views::Widget* PanelStackView::GetWidget() {
  return window_;
}

const views::Widget* PanelStackView::GetWidget() const {
  return window_;
}

void PanelStackView::DeleteDelegate() {
  delete this;
}

void PanelStackView::OnWidgetDestroying(views::Widget* widget) {
  window_ = NULL;
}

void PanelStackView::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
#if defined(OS_WIN)
  if (active && thumbnailer_)
    thumbnailer_->Stop();
#endif
}

void PanelStackView::OnNativeFocusChange(gfx::NativeView focused_before,
                                         gfx::NativeView focused_now) {
  // When the user selects the stacked panels via ALT-TAB or WIN-TAB, the
  // background stack window, instead of the foreground panel window, receives
  // WM_SETFOCUS message. To deal with this, we listen to the focus change event
  // and activate the most recently active panel.
#if defined(OS_WIN)
  if (!panels_.empty() && focused_now == window_->GetNativeView()) {
    Panel* panel_to_focus =
        panels_.front()->stack()->most_recently_active_panel();
    if (panel_to_focus)
      panel_to_focus->Activate();
  }
#endif
}

void PanelStackView::AnimationEnded(const ui::Animation* animation) {
  bounds_updates_started_ = false;

  PanelManager* panel_manager = PanelManager::GetInstance();
  for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
       iter != bounds_updates_.end(); ++iter) {
    panel_manager->OnPanelAnimationEnded(iter->first);
  }
  bounds_updates_.clear();

  delegate_->PanelBoundsBatchUpdateCompleted();
}

void PanelStackView::AnimationProgressed(const ui::Animation* animation) {
  UpdatePanelsBounds();
}

void PanelStackView::UpdatePanelsBounds() {
#if defined(OS_WIN)
  // Add an extra count for the background stack window.
  HDWP defer_update = ::BeginDeferWindowPos(bounds_updates_.size() + 1);
#endif

  // Update the bounds for each panel in the update list.
  gfx::Rect enclosing_bounds;
  for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
       iter != bounds_updates_.end(); ++iter) {
    Panel* panel = iter->first;
    gfx::Rect target_bounds = panel->GetBounds();
    gfx::Rect current_bounds;
    if (bounds_animator_ && bounds_animator_->is_animating()) {
      current_bounds = bounds_animator_->CurrentValueBetween(
          iter->second, target_bounds);
    } else {
      current_bounds = target_bounds;
    }

    PanelView* panel_view = static_cast<PanelView*>(panel->native_panel());
#if defined(OS_WIN)
    DeferUpdateNativeWindowBounds(defer_update,
                                  panel_view->window(),
                                  current_bounds);
#else
    panel_view->SetPanelBoundsInstantly(current_bounds);
#endif

    enclosing_bounds = UnionRects(enclosing_bounds, current_bounds);
  }

  // Compute the stack window bounds that enclose those panels that are not
  // in the batch update list.
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    if (bounds_updates_.find(panel) == bounds_updates_.end())
      enclosing_bounds = UnionRects(enclosing_bounds, panel->GetBounds());
  }

  // Update the bounds of the background stack window.
#if defined(OS_WIN)
  DeferUpdateNativeWindowBounds(defer_update, window_, enclosing_bounds);
#else
  window_->SetBounds(enclosing_bounds);
#endif

#if defined(OS_WIN)
  ::EndDeferWindowPos(defer_update);
#endif
}

void PanelStackView::UpdateStackWindowBounds() {
  gfx::Rect enclosing_bounds;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    enclosing_bounds = UnionRects(enclosing_bounds, panel->GetBounds());
  }
  window_->SetBounds(enclosing_bounds);
}

// static
void PanelStackView::MakeStackWindowOwnPanelWindow(
    Panel* panel, PanelStackView* stack_window) {
#if defined(OS_WIN)
  // The panel widget window might already be gone when a panel is closed.
  views::Widget* panel_window =
      static_cast<PanelView*>(panel->native_panel())->window();
  if (!panel_window)
    return;

  HWND native_panel_window = views::HWNDForWidget(panel_window);
  HWND native_stack_window =
      stack_window ? views::HWNDForWidget(stack_window->window_) : NULL;

  // The extended style WS_EX_APPWINDOW is used to force a top-level window onto
  // the taskbar. In order for multiple stacked panels to appear as one, this
  // bit needs to be cleared.
  int value = ::GetWindowLong(native_panel_window, GWL_EXSTYLE);
  ::SetWindowLong(
      native_panel_window,
      GWL_EXSTYLE,
      native_stack_window ? (value & ~WS_EX_APPWINDOW)
                          : (value | WS_EX_APPWINDOW));

  // All the windows that share the same owner window will appear as a single
  // window on the taskbar.
  ::SetWindowLongPtr(native_panel_window,
                     GWLP_HWNDPARENT,
                     reinterpret_cast<LONG>(native_stack_window));

  // Make sure the background stack window always stays behind the panel window.
  if (native_stack_window) {
    ::SetWindowPos(native_stack_window, native_panel_window, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
  }

#else
  NOTIMPLEMENTED();
#endif
}

void PanelStackView::EnsureWindowCreated() {
  if (window_)
    return;

  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.remove_standard_frame = true;
  // Empty size is not allowed so a temporary small size is passed. SetBounds
  // will be called later to update the bounds.
  params.bounds = gfx::Rect(0, 0, 1, 1);
  window_->Init(params);
  window_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  window_->set_focus_on_creation(false);
  window_->AddObserver(this);
  window_->ShowInactive();

#if defined(OS_WIN)
  DCHECK(!panels_.empty());
  Panel* panel = panels_.front();
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppModelIdForProfile(UTF8ToWide(panel->app_name()),
                                                panel->profile()->GetPath()),
      views::HWNDForWidget(window_));
#endif

  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

#if defined(OS_WIN)
void PanelStackView::CaptureThumbnailForLivePreview() {
  // Live preview is only available since Windows 7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  HWND native_window = views::HWNDForWidget(window_);

  if (!thumbnailer_.get()) {
    DCHECK(native_window);
    thumbnailer_.reset(new TaskbarWindowThumbnailerWin(native_window));
    ui::HWNDSubclass::AddFilterToTarget(native_window, thumbnailer_.get());
  }

  std::vector<HWND> native_panel_windows;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    native_panel_windows.push_back(
        views::HWNDForWidget(
            static_cast<PanelView*>(panel->native_panel())->window()));
  }
  thumbnailer_->Start(native_panel_windows);
}

void PanelStackView::DeferUpdateNativeWindowBounds(HDWP defer_window_pos_info,
                                                   views::Widget* window,
                                                   const gfx::Rect& bounds) {
  ::DeferWindowPos(defer_window_pos_info,
                    views::HWNDForWidget(window),
                    NULL,
                    bounds.x(),
                    bounds.y(),
                    bounds.width(),
                    bounds.height(),
                    SWP_NOACTIVATE | SWP_NOZORDER);
}
#endif
