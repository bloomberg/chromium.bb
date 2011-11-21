// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragged_tab_view.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/views/tabs/native_view_photobooth.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/canvas_skia.h"
#include "views/widget/widget.h"

#if defined(USE_AURA)
#include "views/widget/native_widget_aura.h"
#elif defined(OS_WIN)
#include "views/widget/native_widget_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

static const int kTransparentAlpha = 200;
static const int kOpaqueAlpha = 255;
static const int kDragFrameBorderSize = 2;
static const int kTwiceDragFrameBorderSize = 2 * kDragFrameBorderSize;
static const float kScalingFactor = 0.5;
static const SkColor kDraggedTabBorderColor = SkColorSetRGB(103, 129, 162);

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, public:

DraggedTabView::DraggedTabView(const std::vector<views::View*>& renderers,
                               const std::vector<gfx::Rect>& renderer_bounds,
                               const gfx::Point& mouse_tab_offset,
                               const gfx::Size& contents_size,
                               NativeViewPhotobooth* photobooth)
    : renderers_(renderers),
      renderer_bounds_(renderer_bounds),
      show_contents_on_drag_(true),
      mouse_tab_offset_(mouse_tab_offset),
      photobooth_(photobooth),
      contents_size_(contents_size) {
  set_parent_owned(false);

  container_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.keep_on_top = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(PreferredContainerSize());
  container_->Init(params);
  container_->SetContentsView(this);
#if defined(OS_WIN) && !defined(USE_AURA)
  static_cast<views::NativeWidgetWin*>(container_->native_widget())->
      set_can_update_layered_window(false);

  BOOL drag;
  if ((::SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0, &drag, 0) != 0) &&
      (drag == FALSE)) {
    show_contents_on_drag_ = false;
  }
#endif
  container_->SetOpacity(kTransparentAlpha);
  container_->SetBounds(gfx::Rect(gfx::Point(), params.bounds.size()));
}

DraggedTabView::~DraggedTabView() {
  parent()->RemoveChildView(this);
  container_->CloseNow();
  STLDeleteElements(&renderers_);
}

void DraggedTabView::MoveTo(const gfx::Point& screen_point) {
  int x;
  if (base::i18n::IsRTL()) {
    // On RTL locales, a dragged tab (when it is not attached to a tab strip)
    // is rendered using a right-to-left orientation so we should calculate the
    // window position differently.
    gfx::Size ps = GetPreferredSize();
    x = screen_point.x() + ScaleValue(mouse_tab_offset_.x() - ps.width());
  } else {
    x = screen_point.x() - ScaleValue(mouse_tab_offset_.x());
  }
  int y = screen_point.y() - ScaleValue(mouse_tab_offset_.y());

#if defined(OS_WIN) && !defined(USE_AURA)
  // TODO(beng): make this cross-platform
  int show_flags = container_->IsVisible() ? SWP_NOZORDER : SWP_SHOWWINDOW;
  SetWindowPos(container_->GetNativeView(), HWND_TOP, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | show_flags);
#else
  gfx::Rect bounds = container_->GetWindowScreenBounds();
  container_->SetBounds(gfx::Rect(x, y, bounds.width(), bounds.height()));
  if (!container_->IsVisible())
    container_->Show();
#endif
}

void DraggedTabView::Update() {
  SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabView, views::View overrides:

void DraggedTabView::OnPaint(gfx::Canvas* canvas) {
  if (show_contents_on_drag_)
    PaintDetachedView(canvas);
  else
    PaintFocusRect(canvas);
}

void DraggedTabView::Layout() {
  int max_width = GetPreferredSize().width();
  for (size_t i = 0; i < renderers_.size(); ++i) {
    gfx::Rect bounds = renderer_bounds_[i];
    bounds.set_y(0);
    if (base::i18n::IsRTL())
      bounds.set_x(max_width - bounds.x() - bounds.width());
    renderers_[i]->SetBoundsRect(bounds);
  }
}

gfx::Size DraggedTabView::GetPreferredSize() {
  DCHECK(!renderer_bounds_.empty());
  int max_renderer_x = renderer_bounds_.back().right();
  int width = std::max(max_renderer_x, contents_size_.width()) +
      kTwiceDragFrameBorderSize;
  int height = renderer_bounds_.back().height() + kDragFrameBorderSize +
      contents_size_.height();
  return gfx::Size(width, height);
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabView, private:

void DraggedTabView::PaintDetachedView(gfx::Canvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  gfx::CanvasSkia scale_canvas(ps.width(), ps.height(), false);
  SkBitmap& bitmap_device = const_cast<SkBitmap&>(
      skia::GetTopDevice(*scale_canvas.sk_canvas())->accessBitmap(true));
  bitmap_device.eraseARGB(0, 0, 0, 0);

  int tab_height = renderer_bounds_.back().height();
  scale_canvas.FillRect(kDraggedTabBorderColor,
                        gfx::Rect(0, tab_height - kDragFrameBorderSize,
                                  ps.width(), ps.height() - tab_height));
  gfx::Rect image_rect(kDragFrameBorderSize,
                       tab_height,
                       ps.width() - kTwiceDragFrameBorderSize,
                       contents_size_.height());
  scale_canvas.FillRect(SK_ColorBLACK, image_rect);
  photobooth_->PaintScreenshotIntoCanvas(&scale_canvas, image_rect);
  for (size_t i = 0; i < renderers_.size(); ++i)
    renderers_[i]->Paint(&scale_canvas);

  SkIRect subset;
  subset.set(0, 0, ps.width(), ps.height());
  SkBitmap mipmap = scale_canvas.ExtractBitmap();
  mipmap.buildMipMap(true);

  SkShader* bitmap_shader =
      SkShader::CreateBitmapShader(mipmap, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode);

  SkMatrix shader_scale;
  shader_scale.setScale(kScalingFactor, kScalingFactor);
  bitmap_shader->setLocalMatrix(shader_scale);

  SkPaint paint;
  paint.setShader(bitmap_shader);
  paint.setAntiAlias(true);
  bitmap_shader->unref();

  SkRect rc;
  rc.fLeft = 0;
  rc.fTop = 0;
  rc.fRight = SkIntToScalar(ps.width());
  rc.fBottom = SkIntToScalar(ps.height());
  canvas->GetSkCanvas()->drawRect(rc, paint);
}

void DraggedTabView::PaintFocusRect(gfx::Canvas* canvas) {
  gfx::Size ps = GetPreferredSize();
  canvas->DrawFocusRect(
      gfx::Rect(0, 0,
                static_cast<int>(ps.width() * kScalingFactor),
                static_cast<int>(ps.height() * kScalingFactor)));
}

gfx::Size DraggedTabView::PreferredContainerSize() {
  gfx::Size ps = GetPreferredSize();
  return gfx::Size(ScaleValue(ps.width()), ScaleValue(ps.height()));
}

int DraggedTabView::ScaleValue(int value) {
  return static_cast<int>(value * kScalingFactor);
}
