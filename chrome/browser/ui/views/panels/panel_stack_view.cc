// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/panel_stack_view.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/views/panels/panel_view.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {
// These values are experimental and subjective.
const int kDefaultFramerateHz = 50;
const int kSetBoundsAnimationMs = 180;

// The widget window that acts as a background window for the stack of panels.
class PanelStackWindow : public views::WidgetObserver,
                         public views::WidgetDelegateView {
 public:
  PanelStackWindow(const gfx::Rect& bounds,
                   NativePanelStackWindowDelegate* delegate);
  ~PanelStackWindow() override;

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // Overridden from views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  views::Widget* window_;  // Weak pointer, own us.
  NativePanelStackWindowDelegate* delegate_;  // Weak pointer.

  DISALLOW_COPY_AND_ASSIGN(PanelStackWindow);
};

PanelStackWindow::PanelStackWindow(const gfx::Rect& bounds,
                                   NativePanelStackWindowDelegate* delegate)
    : window_(NULL),
      delegate_(delegate) {
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.remove_standard_frame = true;
  params.bounds = bounds;
  window_->Init(params);
  window_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  window_->set_focus_on_creation(false);
  window_->AddObserver(this);
  window_->ShowInactive();
}

PanelStackWindow::~PanelStackWindow() {
}

base::string16 PanelStackWindow::GetWindowTitle() const {
  return delegate_ ? delegate_->GetTitle() : base::string16();
}

gfx::ImageSkia PanelStackWindow::GetWindowAppIcon() {
  if (delegate_) {
    gfx::Image app_icon = delegate_->GetIcon();
    if (!app_icon.IsEmpty())
      return *app_icon.ToImageSkia();
  }
  return gfx::ImageSkia();
}

gfx::ImageSkia PanelStackWindow::GetWindowIcon() {
  return GetWindowAppIcon();
}

views::Widget* PanelStackWindow::GetWidget() {
  return window_;
}

const views::Widget* PanelStackWindow::GetWidget() const {
  return window_;
}

void PanelStackWindow::OnWidgetClosing(views::Widget* widget) {
  delegate_ = NULL;
}

void PanelStackWindow::OnWidgetDestroying(views::Widget* widget) {
  window_ = NULL;
}

}  // namespace

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
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

PanelStackView::~PanelStackView() {
#if defined(OS_WIN)
  ui::HWNDSubclass::RemoveFilterFromAllTargets(this);
#endif
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
  if (IsAnimatingPanelBounds()) {
    // This panel is gone.
    bounds_updates_.erase(panel);

    // Abort the ongoing animation.
    bounds_animator_->Stop();
  }

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
    NotifyBoundsUpdateCompleted();
    return;
  }

  bounds_animator_.reset(new gfx::LinearAnimation(
      PanelManager::AdjustTimeInterval(kSetBoundsAnimationMs),
      kDefaultFramerateHz,
      this));
  bounds_animator_->Start();
}

void PanelStackView::NotifyBoundsUpdateCompleted() {
  delegate_->PanelBoundsBatchUpdateCompleted();

#if defined(OS_WIN)
  // Refresh the thumbnail each time when any bounds updates are done.
  RefreshLivePreviewThumbnail();
#endif
}

bool PanelStackView::IsAnimatingPanelBounds() const {
  return bounds_updates_started_ && animate_bounds_updates_;
}

void PanelStackView::Minimize() {
#if defined(OS_WIN)
  // When the stack window is minimized by the system, its snapshot could not
  // be obtained. We need to capture the snapshot before the minimization.
  if (thumbnailer_)
    thumbnailer_->CaptureSnapshot();
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

#if defined(OS_WIN)
  // Refresh the thumbnail when a panel could change something for the
  // attention.
  RefreshLivePreviewThumbnail();

  if (draw_attention) {
    // The default implementation of Widget::FlashFrame only flashes 5 times.
    // We need more than that.
    FLASHWINFO fwi;
    fwi.cbSize = sizeof(fwi);
    fwi.hwnd = views::HWNDForWidget(window_);
    fwi.dwFlags = FLASHW_ALL;
    fwi.uCount = panel::kNumberOfTimesToFlashPanelForAttention;
    fwi.dwTimeout = 0;
    ::FlashWindowEx(&fwi);
  } else {
    // Calling FlashWindowEx with FLASHW_STOP flag does not always work.
    // Occasionally the taskbar icon could still remain in the flashed state.
    // To work around this problem, we recreate the underlying window.
    views::Widget* old_window = window_;
    window_ = CreateWindowWithBounds(GetStackWindowBounds());

    // New background window should also be minimized if the old one is.
    if (old_window->IsMinimized())
      window_->Minimize();

    // Make sure the new background window stays at the same z-order as the old
    // one.
    ::SetWindowPos(views::HWNDForWidget(window_),
                   views::HWNDForWidget(old_window),
                   0, 0, 0, 0,
                   SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    for (Panels::const_iterator iter = panels_.begin();
         iter != panels_.end(); ++iter) {
      MakeStackWindowOwnPanelWindow(*iter, this);
    }

    // Serve the snapshot to the new backgroud window.
    if (thumbnailer_.get())
      thumbnailer_->ReplaceWindow(views::HWNDForWidget(window_));

    window_->UpdateWindowTitle();
    window_->UpdateWindowIcon();
    old_window->Close();
  }
#else
  window_->FlashFrame(draw_attention);
#endif
}

void PanelStackView::OnPanelActivated(Panel* panel) {
  // Nothing to do.
}

void PanelStackView::OnNativeFocusChanged(gfx::NativeView focused_now) {
  // When the user selects the stacked panels via ALT-TAB or WIN-TAB, the
  // background stack window, instead of the foreground panel window, receives
  // WM_SETFOCUS message. To deal with this, we listen to the focus change event
  // and activate the most recently active panel.
  // Note that OnNativeFocusChanged might be called when window_ has not been
  // created yet.
#if defined(OS_WIN)
  if (!panels_.empty() && window_ && focused_now == window_->GetNativeView()) {
    Panel* panel_to_focus =
        panels_.front()->stack()->most_recently_active_panel();
    if (panel_to_focus)
      panel_to_focus->Activate();
  }
#endif
}

void PanelStackView::AnimationEnded(const gfx::Animation* animation) {
  bounds_updates_started_ = false;

  PanelManager* panel_manager = PanelManager::GetInstance();
  for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
       iter != bounds_updates_.end(); ++iter) {
    panel_manager->OnPanelAnimationEnded(iter->first);
  }
  bounds_updates_.clear();

  NotifyBoundsUpdateCompleted();
}

void PanelStackView::AnimationCanceled(const gfx::Animation* animation) {
  // When the animation is aborted due to something like one of panels is gone,
  // update panels to their taget bounds immediately.
  UpdatePanelsBounds();

  AnimationEnded(animation);
}

void PanelStackView::AnimationProgressed(const gfx::Animation* animation) {
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

gfx::Rect PanelStackView::GetStackWindowBounds() const {
  gfx::Rect enclosing_bounds;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    enclosing_bounds = UnionRects(enclosing_bounds, panel->GetBounds());
  }
  return enclosing_bounds;
}

void PanelStackView::UpdateStackWindowBounds() {
  window_->SetBounds(GetStackWindowBounds());

#if defined(OS_WIN)
  // Refresh the thumbnail each time whne the stack window is changed, due to
  // adding or removing a panel.
  RefreshLivePreviewThumbnail();
#endif
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
                     reinterpret_cast<LONG_PTR>(native_stack_window));

  // Make sure the background stack window always stays behind the panel window.
  if (native_stack_window) {
    ::SetWindowPos(native_stack_window, native_panel_window, 0, 0, 0, 0,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
  }

#else
  NOTIMPLEMENTED();
#endif
}

views::Widget* PanelStackView::CreateWindowWithBounds(const gfx::Rect& bounds) {
  PanelStackWindow* stack_window = new PanelStackWindow(bounds, delegate_);
  views::Widget* window = stack_window->GetWidget();

#if defined(OS_WIN)
  DCHECK(!panels_.empty());
  Panel* panel = panels_.front();
  ui::win::SetAppIdForWindow(
      shell_integration::GetAppModelIdForProfile(
          base::UTF8ToWide(panel->app_name()), panel->profile()->GetPath()),
      views::HWNDForWidget(window));

  // Remove the filter for old window in case that we're recreating the window.
  ui::HWNDSubclass::RemoveFilterFromAllTargets(this);

  // Listen to WM_MOVING message in order to move all panels windows on top of
  // the background window altogether when the background window is being moved
  // by the user.
  ui::HWNDSubclass::AddFilterToTarget(views::HWNDForWidget(window), this);
#endif

  return window;
}

void PanelStackView::EnsureWindowCreated() {
  if (window_)
    return;

  // Empty size is not allowed so a temporary small size is passed. SetBounds
  // will be called later to update the bounds.
  window_ = CreateWindowWithBounds(gfx::Rect(0, 0, 1, 1));

#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    HWND native_window = views::HWNDForWidget(window_);
    thumbnailer_.reset(new TaskbarWindowThumbnailerWin(native_window, this));
    thumbnailer_->Start();
  }
#endif
}

#if defined(OS_WIN)
bool PanelStackView::FilterMessage(HWND hwnd,
                                   UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param,
                                   LRESULT* l_result) {
  switch (message) {
    case WM_MOVING:
      // When the background window is being moved by the user, all panels
      // should also move.
      gfx::Rect new_stack_bounds(*(reinterpret_cast<LPRECT>(l_param)));
      MovePanelsBy(
          new_stack_bounds.origin() - panels_.front()->GetBounds().origin());
      break;
  }
  return false;
}

std::vector<HWND> PanelStackView::GetSnapshotWindowHandles() const {
  std::vector<HWND> native_panel_windows;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    native_panel_windows.push_back(
        views::HWNDForWidget(
            static_cast<PanelView*>(panel->native_panel())->window()));
  }
  return native_panel_windows;
}

void PanelStackView::RefreshLivePreviewThumbnail() {
  // Don't refresh the thumbnail when the stack window is system minimized
  // because the snapshot could not be retrieved.
  if (!thumbnailer_.get() || IsMinimized())
    return;
  thumbnailer_->InvalidateSnapshot();
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
