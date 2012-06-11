// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/shell_window_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/ui_resources_standard.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_sk_region.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/base/win/shell.h"
#endif

#if defined(USE_ASH)
#include "ash/wm/custom_frame_view_ash.h"
#endif

// Number of pixels around the edge of the window that can be dragged to
// resize the window.
static const int kResizeBorderWidth = 5;
// Height of the chrome-style caption, in pixels.
static const int kCaptionHeight = 25;

class ShellWindowFrameView : public views::NonClientFrameView {
 public:
  explicit ShellWindowFrameView(ShellWindowViews* window);
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
  virtual gfx::Size GetMinimumSize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWindowFrameView);

  ShellWindowViews* window_;
};

ShellWindowFrameView::ShellWindowFrameView(ShellWindowViews* window)
  : window_(window) {
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

gfx::Size ShellWindowFrameView::GetMinimumSize() {
  return window_->minimum_size_;
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

namespace {

class TitleBarBackground : public views::Background {
 public:
  TitleBarBackground() {}
  virtual ~TitleBarBackground() {}
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorWHITE);
    gfx::Path path;
    int radius = 1;
    int width = view->width();
    int height = view->height();
    path.moveTo(0, radius);
    path.lineTo(radius, 0);
    path.lineTo(width - radius - 1, 0);
    path.lineTo(width, radius + 1);
    path.lineTo(width, height);
    path.lineTo(0, height);
    path.close();
    canvas->DrawPath(path, paint);
  }
};

class TitleBarView : public views::View {
 public:
  explicit TitleBarView(views::ButtonListener* listener);
  virtual ~TitleBarView();
};

TitleBarView::TitleBarView(views::ButtonListener* listener) {
  set_background(new TitleBarBackground());
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  views::ImageButton* close_button = new views::ImageButton(listener);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetNativeImageNamed(IDR_CLOSE_BAR).ToImageSkia());
  close_button->SetImage(views::CustomButton::BS_HOT,
      rb.GetNativeImageNamed(IDR_CLOSE_BAR_H).ToImageSkia());
  close_button->SetImage(views::CustomButton::BS_PUSHED,
      rb.GetNativeImageNamed(IDR_CLOSE_BAR_P).ToImageSkia());
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(0, 0, 0,
      (kCaptionHeight - close_button->GetPreferredSize().height()) / 2);
  SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(1, 1);
  columns->AddColumn(
      views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
      views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(close_button);
}

TitleBarView::~TitleBarView() {
}

};  // namespace

ShellWindowViews::ShellWindowViews(Profile* profile,
                                   const extensions::Extension* extension,
                                   const GURL& url,
                                   const ShellWindow::CreateParams& win_params)
    : ShellWindow(profile, extension, url),
      title_view_(NULL),
      web_view_(NULL),
      is_fullscreen_(false),
      use_custom_frame_(
          win_params.frame == ShellWindow::CreateParams::FRAME_CUSTOM) {
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.remove_standard_frame = true;
  params.bounds = win_params.bounds;
  minimum_size_ = win_params.minimum_size;
  if (!use_custom_frame_)
    params.bounds.set_height(params.bounds.height() + kCaptionHeight);
  window_->Init(params);
#if defined(OS_WIN) && !defined(USE_AURA)
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      extension->id());
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppId(UTF8ToWide(app_name),
      profile->GetPath()),
      GetWidget()->GetTopLevelWidget()->GetNativeWindow());
#endif
  OnViewWasResized();

  window_->Show();
}

void ShellWindowViews::ViewHierarchyChanged(
    bool is_add, views::View *parent, views::View *child) {
  if (is_add && child == this) {
    if (!use_custom_frame_) {
      title_view_ = new TitleBarView(this);
      AddChildView(title_view_);
    }
    web_view_ = new views::WebView(NULL);
    AddChildView(web_view_);
    web_view_->SetWebContents(web_contents());
  }
}

void ShellWindowViews::SetFullscreen(bool fullscreen) {
  is_fullscreen_ = fullscreen;
  window_->SetFullscreen(fullscreen);
  // TODO(jeremya) we need to call RenderViewHost::ExitFullscreen() if we
  // ever drop the window out of fullscreen in response to something that
  // wasn't the app calling webkitCancelFullScreen().
}

bool ShellWindowViews::IsFullscreenOrPending() const {
  return is_fullscreen_;
}

ShellWindowViews::~ShellWindowViews() {
  web_view_->SetWebContents(NULL);
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

gfx::NativeWindow ShellWindowViews::GetNativeWindow() {
  return window_->GetNativeWindow();
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
  OnNativeClose();
}

void ShellWindowViews::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  Close();
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
  return new ShellWindowFrameView(this);
#endif
}

string16 ShellWindowViews::GetWindowTitle() const {
  return GetTitle();
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
  // Set the window shape of the RWHV.
  DCHECK(window_);
  DCHECK(web_view_);
  gfx::Size sz = web_view_->size();
  int height = sz.height(), width = sz.width();
  int radius = 1;
  gfx::Path path;
  if (window_->IsMaximized() || window_->IsFullscreen()) {
    // Don't round the corners when the window is maximized or fullscreen.
    path.addRect(0, 0, width, height);
  } else {
    if (use_custom_frame_) {
      path.moveTo(0, radius);
      path.lineTo(radius, 0);
      path.lineTo(width - radius, 0);
      path.lineTo(width, radius);
    } else {
      // Don't round the top corners in chrome-style frame mode.
      path.moveTo(0, 0);
      path.lineTo(width, 0);
    }
    path.lineTo(width, height - radius - 1);
    path.lineTo(width - radius - 1, height);
    path.lineTo(radius + 1, height);
    path.lineTo(0, height - radius - 1);
    path.close();
  }
  SetWindowRgn(web_contents()->GetNativeView(), path.CreateNativeRegion(), 1);

  SkRegion* rgn = new SkRegion;
  if (!window_->IsFullscreen()) {
    if (caption_region_.Get())
      rgn->op(*caption_region_.Get(), SkRegion::kUnion_Op);
    if (!window_->IsMaximized()) {
      if (use_custom_frame_)
        rgn->op(0, 0, width, kResizeBorderWidth, SkRegion::kUnion_Op);
      rgn->op(0, 0, kResizeBorderWidth, height, SkRegion::kUnion_Op);
      rgn->op(width - kResizeBorderWidth, 0, width, height,
          SkRegion::kUnion_Op);
      rgn->op(0, height - kResizeBorderWidth, width, height,
          SkRegion::kUnion_Op);
    }
  }
  web_contents()->GetRenderViewHost()->GetView()->SetClickthroughRegion(rgn);
#endif
}

void ShellWindowViews::Layout() {
  DCHECK(web_view_);
  if (use_custom_frame_) {
    web_view_->SetBounds(0, 0, width(), height());
  } else {
    DCHECK(title_view_);
    if (window_->IsFullscreen()) {
      title_view_->SetVisible(false);
      web_view_->SetBounds(0, 0, width(), height());
    } else {
      title_view_->SetVisible(true);
      title_view_->SetBounds(0, 0, width(), kCaptionHeight);
      web_view_->SetBounds(0, kCaptionHeight,
          width(), height() - kCaptionHeight);
    }
  }
  OnViewWasResized();
}

void ShellWindowViews::UpdateWindowTitle() {
  window_->UpdateWindowTitle();
}

// static
ShellWindow* ShellWindow::CreateImpl(Profile* profile,
                                     const extensions::Extension* extension,
                                     const GURL& url,
                                     const ShellWindow::CreateParams params) {
  return new ShellWindowViews(profile, extension, url, params);
}
