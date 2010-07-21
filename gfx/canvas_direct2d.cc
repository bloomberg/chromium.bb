// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/canvas_direct2d.h"

#include "gfx/rect.h"

namespace {

// Converts a SkColor to a ColorF.
D2D1_COLOR_F SkColorToColorF(SkColor color) {
  return D2D1::ColorF(static_cast<float>(SkColorGetR(color)) / 0xFF,
                      static_cast<float>(SkColorGetG(color)) / 0xFF,
                      static_cast<float>(SkColorGetB(color)) / 0xFF,
                      static_cast<float>(SkColorGetA(color)) / 0xFF);
}

D2D1_RECT_F RectToRectF(int x, int y, int w, int h) {
  return D2D1::RectF(static_cast<float>(x), static_cast<float>(y),
                     static_cast<float>(x + w), static_cast<float>(y + h));
}

D2D1_RECT_F RectToRectF(const gfx::Rect& rect) {
  return RectToRectF(rect.x(), rect.y(), rect.width(), rect.height());
}

}  // namespace

namespace gfx {

// static
ID2D1Factory* CanvasDirect2D::d2d1_factory_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// CanvasDirect2D, public:

CanvasDirect2D::CanvasDirect2D(ID2D1RenderTarget* rt) : rt_(rt) {
  // A RenderState entry is pushed onto the stack to track the clip count prior
  // to any calls to Save*().
  state_.push(RenderState());
  rt_->BeginDraw();
}

CanvasDirect2D::~CanvasDirect2D() {
  // Unwind any clips that were pushed outside of any Save*()/Restore() pairs.
  int clip_count = state_.top().clip_count;
  for (int i = 0; i < clip_count; ++i)
    rt_->PopAxisAlignedClip();
  rt_->EndDraw();
}

// static
ID2D1Factory* CanvasDirect2D::GetD2D1Factory() {
  if (!d2d1_factory_)
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d1_factory_);
  return d2d1_factory_;
}

////////////////////////////////////////////////////////////////////////////////
// CanvasDirect2D, Canvas implementation:

void CanvasDirect2D::Save() {
  SaveInternal(NULL);
}

void CanvasDirect2D::SaveLayerAlpha(uint8 alpha) {
  SaveLayerAlpha(alpha, gfx::Rect());
}

void CanvasDirect2D::SaveLayerAlpha(uint8 alpha,
                                    const gfx::Rect& layer_bounds) {
  D2D1_RECT_F bounds = D2D1::InfiniteRect();
  if (!layer_bounds.IsEmpty())
    bounds = RectToRectF(layer_bounds);
  ID2D1Layer* layer = NULL;
  HRESULT hr = rt_->CreateLayer(NULL, &layer);
  if (SUCCEEDED(hr)) {
    rt_->PushLayer(D2D1::LayerParameters(bounds,
                                         NULL,
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                         D2D1::IdentityMatrix(),
                                         static_cast<float>(alpha) / 0xFF,
                                         NULL,
                                         D2D1_LAYER_OPTIONS_NONE),
                   layer);
  }
  SaveInternal(layer);
}

void CanvasDirect2D::Restore() {
  ID2D1Layer* layer = state_.top().layer;
  if (layer) {
    rt_->PopLayer();
    layer->Release();
  }

  int clip_count = state_.top().clip_count;
  for (int i = 0; i < clip_count; ++i)
    rt_->PopAxisAlignedClip();

  state_.pop();
  // The state_ stack should never be empty - we should always have at least one
  // entry to hold a clip count when there is no active save/restore entry.
  CHECK(!state_.empty()) << "Called Restore() once too often!";

  rt_->RestoreDrawingState(drawing_state_block_);
}

bool CanvasDirect2D::ClipRectInt(int x, int y, int w, int h) {
  rt_->PushAxisAlignedClip(RectToRectF(x, y, w, h),
                           D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  // Increment the clip count so the call to PushAxisAlignedClip() can be
  // balanced with a call to PopAxisAlignedClip in the next Restore().
  ++state_.top().clip_count;
  return w > 0 && h > 0;
}

void CanvasDirect2D::TranslateInt(int x, int y) {
  rt_->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(x),
                                                  static_cast<float>(y)));
}

void CanvasDirect2D::ScaleInt(int x, int y) {
  rt_->SetTransform(D2D1::Matrix3x2F::Scale(static_cast<float>(x),
                                            static_cast<float>(y)));
}

void CanvasDirect2D::FillRectInt(int x, int y, int w, int h,
                                 const SkPaint& paint) {
}

void CanvasDirect2D::FillRectInt(const SkColor& color, int x, int y, int w,
                                 int h) {
  ScopedComPtr<ID2D1SolidColorBrush> solid_brush;
  rt_->CreateSolidColorBrush(SkColorToColorF(color), solid_brush.Receive());
  rt_->FillRectangle(RectToRectF(x, y, w, h), solid_brush);
}

void CanvasDirect2D::DrawRectInt(const SkColor& color, int x, int y, int w,
                                 int h) {

}

void CanvasDirect2D::DrawRectInt(const SkColor& color, int x, int y, int w,
                                 int h, SkXfermode::Mode mode) {

}

void CanvasDirect2D::DrawLineInt(const SkColor& color, int x1, int y1, int x2,
                                 int y2) {

}

void CanvasDirect2D::DrawBitmapInt(const SkBitmap& bitmap, int x, int y) {
}

void CanvasDirect2D::DrawBitmapInt(const SkBitmap& bitmap, int x, int y,
                                   const SkPaint& paint) {

}

void CanvasDirect2D::DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y,
                                   int src_w, int src_h, int dest_x, int dest_y,
                                   int dest_w, int dest_h, bool filter) {

}

void CanvasDirect2D::DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y,
                                   int src_w, int src_h, int dest_x, int dest_y,
                                   int dest_w, int dest_h, bool filter,
                                   const SkPaint& paint) {

}

void CanvasDirect2D::DrawStringInt(const std::wstring& text,
                                   const gfx::Font& font,
                                   const SkColor& color, int x, int y, int w,
                                   int h) {

}

void CanvasDirect2D::DrawStringInt(const std::wstring& text,
                                   const gfx::Font& font,
                                   const SkColor& color,
                                   const gfx::Rect& display_rect) {

}

void CanvasDirect2D::DrawStringInt(const std::wstring& text,
                                   const gfx::Font& font,
                                   const SkColor& color,
                                   int x, int y, int w, int h,
                                   int flags) {

}

void CanvasDirect2D::DrawFocusRect(int x, int y, int width, int height) {
}

void CanvasDirect2D::TileImageInt(const SkBitmap& bitmap, int x, int y, int w,
                                  int h) {
}

void CanvasDirect2D::TileImageInt(const SkBitmap& bitmap, int src_x, int src_y,
                                  int dest_x, int dest_y, int w, int h) {

}

gfx::NativeDrawingContext CanvasDirect2D::BeginPlatformPaint() {
  DCHECK(!interop_rt_.get());
  interop_rt_.QueryFrom(rt_);
  HDC dc = NULL;
  if (interop_rt_.get())
    interop_rt_->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &dc);
  return dc;
}

void CanvasDirect2D::EndPlatformPaint() {
  DCHECK(interop_rt_.get());
  interop_rt_->ReleaseDC(NULL);
  interop_rt_.release();
}

CanvasSkia* CanvasDirect2D::AsCanvasSkia() {
  return NULL;
}

const CanvasSkia* CanvasDirect2D::AsCanvasSkia() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// CanvasDirect2D, private:

void CanvasDirect2D::SaveInternal(ID2D1Layer* layer) {
  if (!drawing_state_block_)
    GetD2D1Factory()->CreateDrawingStateBlock(drawing_state_block_.Receive());
  rt_->SaveDrawingState(drawing_state_block_.get());
  state_.push(RenderState(layer));
}

}  // namespace gfx
