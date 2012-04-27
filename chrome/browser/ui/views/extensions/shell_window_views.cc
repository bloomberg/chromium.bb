// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/shell_window_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/extensions/extension.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_sk_region.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/win/shell.h"
#endif

#if defined(USE_ASH)
#include "ash/wm/custom_frame_view_ash.h"
#endif

// Number of pixels around the edge of the window that can be dragged to
// resize the window.
static const int kResizeBorderWidth = 5;

class ShellWindowFrameView : public views::NonClientFrameView {
 public:
  ShellWindowFrameView();
  virtual ~ShellWindowFrameView();

  // views::NonClientFrameView implementation.
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWindowFrameView);
};

ShellWindowFrameView::ShellWindowFrameView() {
}

ShellWindowFrameView::~ShellWindowFrameView() {
}

gfx::Rect ShellWindowFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Rect ShellWindowFrameView::GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int ShellWindowFrameView::NonClientHitTest(const gfx::Point& point) {
  // No resize border when maximized.
  if (GetWidget()->IsMaximized())
    return HTCAPTION;
  int x = point.x();
  int y = point.y();
  if (x <= kResizeBorderWidth) {
    if (y <= kResizeBorderWidth)
      return HTTOPLEFT;
    if (y >= height() - kResizeBorderWidth)
      return HTBOTTOMLEFT;
    return HTLEFT;
  }
  if (x >= width() - kResizeBorderWidth) {
    if (y <= kResizeBorderWidth)
      return HTTOPRIGHT;
    if (y >= height() - kResizeBorderWidth)
      return HTBOTTOMRIGHT;
    return HTRIGHT;
  }
  if (y <= kResizeBorderWidth)
    return HTTOP;
  if (y >= height() - kResizeBorderWidth)
    return HTBOTTOM;
  return HTCAPTION;
}

void ShellWindowFrameView::GetWindowMask(const gfx::Size& size,
                                         gfx::Path* window_mask) {
  // Don't touch it.
}

ShellWindowViews::ShellWindowViews(ExtensionHost* host)
    : ShellWindow(host) {
  host_->view()->SetContainer(this);
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.remove_standard_frame = true;
  gfx::Rect bounds(10, 10, kDefaultWidth, kDefaultHeight);
  params.bounds = bounds;
  window_->Init(params);
#if defined(OS_WIN) && !defined(USE_AURA)
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      host_->extension()->id());
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppId(UTF8ToWide(app_name),
      host_->profile()->GetPath()),
      GetWidget()->GetTopLevelWidget()->GetNativeWindow());
#endif
  AddChildView(host_->view());
  SetLayoutManager(new views::FillLayout);
  Layout();

  window_->Show();
}

ShellWindowViews::~ShellWindowViews() {
}

bool ShellWindowViews::IsActive() const {
  return window_->IsActive();
}

bool ShellWindowViews::IsMaximized() const {
  return window_->IsMaximized();
}

bool ShellWindowViews::IsMinimized() const {
  return window_->IsMinimized();
}

bool ShellWindowViews::IsFullscreen() const {
  return window_->IsFullscreen();
}

gfx::Rect ShellWindowViews::GetRestoredBounds() const {
  return window_->GetRestoredBounds();
}

gfx::Rect ShellWindowViews::GetBounds() const {
  return window_->GetWindowScreenBounds();
}

void ShellWindowViews::Show() {
  if (window_->IsVisible()) {
    window_->Activate();
    return;
  }

  window_->Show();
}

void ShellWindowViews::ShowInactive() {
  if (window_->IsVisible())
    return;
  window_->ShowInactive();
}

void ShellWindowViews::Close() {
  window_->Close();
}

void ShellWindowViews::Activate() {
  window_->Activate();
}

void ShellWindowViews::Deactivate() {
  window_->Deactivate();
}

void ShellWindowViews::Maximize() {
  window_->Maximize();
}

void ShellWindowViews::Minimize() {
  window_->Minimize();
}

void ShellWindowViews::Restore() {
  window_->Restore();
}

void ShellWindowViews::SetBounds(const gfx::Rect& bounds) {
  GetWidget()->SetBounds(bounds);
}

void ShellWindowViews::SetDraggableRegion(SkRegion* region) {
  caption_region_.Set(region);
  OnViewWasResized();
}

void ShellWindowViews::FlashFrame(bool flash) {
  window_->FlashFrame(flash);
}

bool ShellWindowViews::IsAlwaysOnTop() const {
  return false;
}

void ShellWindowViews::DeleteDelegate() {
  delete this;
}

bool ShellWindowViews::CanResize() const {
  return true;
}

bool ShellWindowViews::CanMaximize() const {
  return true;
}

views::View* ShellWindowViews::GetContentsView() {
  return this;
}

views::NonClientFrameView* ShellWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
#if defined(USE_ASH)
  // TODO(jeremya): make this an option when we have HTTRANSPARENT handling in
  // aura.
  ash::CustomFrameViewAsh* frame = new ash::CustomFrameViewAsh();
  frame->Init(widget);
  return frame;
#else
  return new ShellWindowFrameView();
#endif
}

string16 ShellWindowViews::GetWindowTitle() const {
  return UTF8ToUTF16(host_->extension()->name());
}

views::Widget* ShellWindowViews::GetWidget() {
  return window_;
}

const views::Widget* ShellWindowViews::GetWidget() const {
  return window_;
}

void ShellWindowViews::OnViewWasResized() {
  // TODO(jeremya): this doesn't seem like a terribly elegant way to keep the
  // window shape in sync.
#if defined(OS_WIN) && !defined(USE_AURA)
  gfx::Size sz = host_->view()->size();
  int height = sz.height(), width = sz.width();
  int radius = 1;
  gfx::Path path;
  if (GetWidget()->IsMaximized()) {
    // Don't round the corners when the window is maximized.
    path.addRect(0, 0, width, height);
  } else {
    path.moveTo(0, radius);
    path.lineTo(radius, 0);
    path.lineTo(width - radius, 0);
    path.lineTo(width, radius);
    path.lineTo(width, height - radius - 1);
    path.lineTo(width - radius - 1, height);
    path.lineTo(radius + 1, height);
    path.lineTo(0, height - radius - 1);
    path.close();
  }
  SetWindowRgn(host_->view()->native_view(), path.CreateNativeRegion(), 1);

  SkRegion* rgn = new SkRegion;
  if (caption_region_.Get())
    rgn->op(*caption_region_.Get(), SkRegion::kUnion_Op);
  if (!GetWidget()->IsMaximized()) {
    rgn->op(0, 0, width, kResizeBorderWidth, SkRegion::kUnion_Op);
    rgn->op(0, 0, kResizeBorderWidth, height, SkRegion::kUnion_Op);
    rgn->op(width - kResizeBorderWidth, 0, width, height, SkRegion::kUnion_Op);
    rgn->op(0, height - kResizeBorderWidth, width, height, SkRegion::kUnion_Op);
  }
  host_->render_view_host()->GetView()->SetClickthroughRegion(rgn);
#endif
}

// static
ShellWindow* ShellWindow::CreateShellWindow(ExtensionHost* host) {
  return new ShellWindowViews(host);
}
