// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer_impl.h"

#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/debug_rect_history.h"
#include "cc/debug/frame_rate_counter.h"
#include "cc/debug/paint_time_counter.h"
#include "cc/layers/quad_sink.h"
#include "cc/output/renderer.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/tile_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace cc {

static inline SkPaint CreatePaint() {
  SkPaint paint;
#if (SK_R32_SHIFT || SK_B32_SHIFT != 16)
  // The SkCanvas is in RGBA but the shader is expecting BGRA, so we need to
  // swizzle our colors when drawing to the SkCanvas.
  SkColorMatrix swizzle_matrix;
  for (int i = 0; i < 20; ++i)
    swizzle_matrix.fMat[i] = 0;
  swizzle_matrix.fMat[0 + 5 * 2] = 1;
  swizzle_matrix.fMat[1 + 5 * 1] = 1;
  swizzle_matrix.fMat[2 + 5 * 0] = 1;
  swizzle_matrix.fMat[3 + 5 * 3] = 1;

  skia::RefPtr<SkColorMatrixFilter> filter =
      skia::AdoptRef(new SkColorMatrixFilter(swizzle_matrix));
  paint.setColorFilter(filter.get());
#endif
  return paint;
}

HeadsUpDisplayLayerImpl::Graph::Graph(double indicator_value,
                                      double start_upper_bound)
    : value(0.0),
      min(0.0),
      max(0.0),
      current_upper_bound(start_upper_bound),
      default_upper_bound(start_upper_bound),
      indicator(indicator_value) {}

double HeadsUpDisplayLayerImpl::Graph::UpdateUpperBound() {
  double target_upper_bound = std::max(max, default_upper_bound);
  current_upper_bound += (target_upper_bound - current_upper_bound) * 0.5;
  return current_upper_bound;
}

HeadsUpDisplayLayerImpl::HeadsUpDisplayLayerImpl(LayerTreeImpl* tree_impl,
                                                 int id)
    : LayerImpl(tree_impl, id),
      fps_graph_(60.0, 80.0),
      paint_time_graph_(16.0, 48.0),
      typeface_(skia::AdoptRef(
          SkTypeface::CreateFromName("monospace", SkTypeface::kBold))) {}

HeadsUpDisplayLayerImpl::~HeadsUpDisplayLayerImpl() {}

scoped_ptr<LayerImpl> HeadsUpDisplayLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void HeadsUpDisplayLayerImpl::WillDraw(ResourceProvider* resource_provider) {
  LayerImpl::WillDraw(resource_provider);

  if (!hud_texture_)
    hud_texture_ = ScopedResource::create(resource_provider);

  // TODO(danakj): Scale the HUD by device scale to make it more friendly under
  // high DPI.

  // TODO(danakj): The HUD could swap between two textures instead of creating a
  // texture every frame in ubercompositor.
  if (hud_texture_->size() != bounds() ||
      resource_provider->InUseByConsumer(hud_texture_->id()))
    hud_texture_->Free();

  if (!hud_texture_->id()) {
    hud_texture_->Allocate(
        bounds(), GL_RGBA, ResourceProvider::TextureUsageAny);
    // TODO(epenner): This texture was being used before SetPixels was called,
    // which is now not allowed (it's an uninitialized read). This should be
    // fixed and this allocateForTesting() removed.
    // http://crbug.com/166784
    resource_provider->AllocateForTesting(hud_texture_->id());
  }
}

void HeadsUpDisplayLayerImpl::AppendQuads(QuadSink* quad_sink,
                                          AppendQuadsData* append_quads_data) {
  if (!hud_texture_->id())
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());

  gfx::Rect quad_rect(gfx::Point(), bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  const float vertex_opacity[] = { 1.f, 1.f, 1.f, 1.f };
  bool flipped = false;
  scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               hud_texture_->id(),
               premultiplied_alpha,
               uv_top_left,
               uv_bottom_right,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

void HeadsUpDisplayLayerImpl::UpdateHudTexture(
    ResourceProvider* resource_provider) {
  if (!hud_texture_->id())
    return;

  SkISize canvas_size;
  if (hud_canvas_)
    canvas_size = hud_canvas_->getDeviceSize();
  else
    canvas_size.set(0, 0);

  if (canvas_size.fWidth != bounds().width() ||
      canvas_size.fHeight != bounds().height() || !hud_canvas_) {
    bool opaque = false;
    hud_canvas_ = make_scoped_ptr(
        skia::CreateBitmapCanvas(bounds().width(), bounds().height(), opaque));
  }

  UpdateHudContents();

  hud_canvas_->clear(SkColorSetARGB(0, 0, 0, 0));
  DrawHudContents(hud_canvas_.get());

  const SkBitmap* bitmap = &hud_canvas_->getDevice()->accessBitmap(false);
  SkAutoLockPixels locker(*bitmap);

  gfx::Rect layer_rect(bounds());
  DCHECK(bitmap->config() == SkBitmap::kARGB_8888_Config);
  resource_provider->SetPixels(hud_texture_->id(),
                               static_cast<const uint8_t*>(bitmap->getPixels()),
                               layer_rect,
                               layer_rect,
                               gfx::Vector2d());
}

void HeadsUpDisplayLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);

  if (!hud_texture_->id())
    return;

  // FIXME: the following assert will not be true when sending resources to a
  // parent compositor. We will probably need to hold on to hud_texture_ for
  // longer, and have several HUD textures in the pipeline.
  DCHECK(!resource_provider->InUseByConsumer(hud_texture_->id()));
}

void HeadsUpDisplayLayerImpl::DidLoseOutputSurface() { hud_texture_.reset(); }

bool HeadsUpDisplayLayerImpl::LayerIsAlwaysDamaged() const { return true; }

void HeadsUpDisplayLayerImpl::UpdateHudContents() {
  const LayerTreeDebugState& debug_state = layer_tree_impl()->debug_state();

  // Don't update numbers every frame so text is readable.
  base::TimeTicks now = layer_tree_impl()->CurrentFrameTime();
  if (base::TimeDelta(now - time_of_last_graph_update_).InSecondsF() > 0.25f) {
    time_of_last_graph_update_ = now;

    if (debug_state.show_fps_counter) {
      FrameRateCounter* fps_counter = layer_tree_impl()->frame_rate_counter();
      fps_graph_.value = fps_counter->GetAverageFPS();
      fps_counter->GetMinAndMaxFPS(&fps_graph_.min, &fps_graph_.max);
    }

    if (debug_state.continuous_painting) {
      PaintTimeCounter* paint_time_counter =
          layer_tree_impl()->paint_time_counter();
      base::TimeDelta latest, min, max;

      if (paint_time_counter->End())
        latest = paint_time_counter->End()->total_time();
      paint_time_counter->GetMinAndMaxPaintTime(&min, &max);

      paint_time_graph_.value = latest.InMillisecondsF();
      paint_time_graph_.min = min.InMillisecondsF();
      paint_time_graph_.max = max.InMillisecondsF();
    }

    if (debug_state.ShowMemoryStats()) {
      MemoryHistory* memory_history = layer_tree_impl()->memory_history();
      if (memory_history->End())
        memory_entry_ = **memory_history->End();
      else
        memory_entry_ = MemoryHistory::Entry();
    }
  }

  fps_graph_.UpdateUpperBound();
  paint_time_graph_.UpdateUpperBound();
}

void HeadsUpDisplayLayerImpl::DrawHudContents(SkCanvas* canvas) const {
  const LayerTreeDebugState& debug_state = layer_tree_impl()->debug_state();

  if (debug_state.ShowHudRects())
    DrawDebugRects(canvas, layer_tree_impl()->debug_rect_history());

  if (debug_state.show_platform_layer_tree)
    DrawPlatformLayerTree(canvas);

  SkRect area = SkRect::MakeEmpty();
  if (debug_state.continuous_painting) {
    // Don't show the FPS display when continuous painting is enabled, because
    // it would show misleading numbers.
    area = DrawPaintTimeDisplay(
        canvas, layer_tree_impl()->paint_time_counter(), 0, 0);
  } else if (debug_state.show_fps_counter) {
    area =
        DrawFPSDisplay(canvas, layer_tree_impl()->frame_rate_counter(), 0, 0);
  }

  if (debug_state.ShowMemoryStats())
    DrawMemoryDisplay(canvas, 0, area.bottom(), SkMaxScalar(area.width(), 150));
}

void HeadsUpDisplayLayerImpl::DrawText(SkCanvas* canvas,
                                       SkPaint* paint,
                                       const std::string& text,
                                       SkPaint::Align align,
                                       int size,
                                       int x,
                                       int y) const {
  const bool anti_alias = paint->isAntiAlias();
  paint->setAntiAlias(true);

  paint->setTextSize(size);
  paint->setTextAlign(align);
  paint->setTypeface(typeface_.get());
  canvas->drawText(text.c_str(), text.length(), x, y, *paint);

  paint->setAntiAlias(anti_alias);
}

void HeadsUpDisplayLayerImpl::DrawText(SkCanvas* canvas,
                                       SkPaint* paint,
                                       const std::string& text,
                                       SkPaint::Align align,
                                       int size,
                                       const SkPoint& pos) const {
  DrawText(canvas, paint, text, align, size, pos.x(), pos.y());
}

void HeadsUpDisplayLayerImpl::DrawGraphBackground(SkCanvas* canvas,
                                                  SkPaint* paint,
                                                  const SkRect& bounds) const {
  paint->setColor(DebugColors::HUDBackgroundColor());
  canvas->drawRect(bounds, *paint);
}

void HeadsUpDisplayLayerImpl::DrawGraphLines(SkCanvas* canvas,
                                             SkPaint* paint,
                                             const SkRect& bounds,
                                             const Graph& graph) const {
  // Draw top and bottom line.
  paint->setColor(DebugColors::HUDSeparatorLineColor());
  canvas->drawLine(bounds.left(),
                   bounds.top() - 1,
                   bounds.right(),
                   bounds.top() - 1,
                   *paint);
  canvas->drawLine(
      bounds.left(), bounds.bottom(), bounds.right(), bounds.bottom(), *paint);

  // Draw indicator line (additive blend mode to increase contrast when drawn on
  // top of graph).
  paint->setColor(DebugColors::HUDIndicatorLineColor());
  paint->setXfermodeMode(SkXfermode::kPlus_Mode);
  const double indicator_top =
      bounds.height() * (1.0 - graph.indicator / graph.current_upper_bound) -
      1.0;
  canvas->drawLine(bounds.left(),
                   bounds.top() + indicator_top,
                   bounds.right(),
                   bounds.top() + indicator_top,
                   *paint);
  paint->setXfermode(NULL);
}

void HeadsUpDisplayLayerImpl::DrawPlatformLayerTree(SkCanvas* canvas) const {
  const int kFontHeight = 14;
  SkPaint paint = CreatePaint();
  DrawGraphBackground(
      canvas,
      &paint,
      SkRect::MakeXYWH(0, 0, bounds().width(), bounds().height()));

  std::string layer_tree = layer_tree_impl()->layer_tree_as_text();
  std::vector<std::string> lines;
  base::SplitString(layer_tree, '\n', &lines);

  paint.setColor(DebugColors::PlatformLayerTreeTextColor());
  for (size_t i = 0;
       i < lines.size() &&
           static_cast<int>(2 + i * kFontHeight) < bounds().height();
       ++i) {
    DrawText(canvas,
             &paint,
             lines[i],
             SkPaint::kLeft_Align,
             kFontHeight,
             2,
             2 + (i + 1) * kFontHeight);
  }
}

SkRect HeadsUpDisplayLayerImpl::DrawFPSDisplay(
    SkCanvas* canvas,
    const FrameRateCounter* fps_counter,
    int right,
    int top) const {
  const int kPadding = 4;
  const int kGap = 6;

  const int kFontHeight = 15;

  const int kGraphWidth = fps_counter->time_stamp_history_size() - 2;
  const int kGraphHeight = 40;

  const int kHistogramWidth = 37;

  int width = kGraphWidth + kHistogramWidth + 4 * kPadding;
  int height = kFontHeight + kGraphHeight + 4 * kPadding + 2;
  int left = bounds().width() - width - right;
  SkRect area = SkRect::MakeXYWH(left, top, width, height);

  SkPaint paint = CreatePaint();
  DrawGraphBackground(canvas, &paint, area);

  SkRect text_bounds =
      SkRect::MakeXYWH(left + kPadding,
                       top + kPadding,
                       kGraphWidth + kHistogramWidth + kGap + 2,
                       kFontHeight);
  SkRect graph_bounds = SkRect::MakeXYWH(left + kPadding,
                                         text_bounds.bottom() + 2 * kPadding,
                                         kGraphWidth,
                                         kGraphHeight);
  SkRect histogram_bounds = SkRect::MakeXYWH(graph_bounds.right() + kGap,
                                             graph_bounds.top(),
                                             kHistogramWidth,
                                             kGraphHeight);

  const std::string value_text =
      base::StringPrintf("FPS:%5.1f", fps_graph_.value);
  const std::string min_max_text =
      base::StringPrintf("%.0f-%.0f", fps_graph_.min, fps_graph_.max);

  paint.setColor(DebugColors::FPSDisplayTextAndGraphColor());
  DrawText(canvas,
           &paint,
           value_text,
           SkPaint::kLeft_Align,
           kFontHeight,
           text_bounds.left(),
           text_bounds.bottom());
  DrawText(canvas,
           &paint,
           min_max_text,
           SkPaint::kRight_Align,
           kFontHeight,
           text_bounds.right(),
           text_bounds.bottom());

  DrawGraphLines(canvas, &paint, graph_bounds, fps_graph_);

  // Collect graph and histogram data.
  SkPath path;

  const int kHistogramSize = 20;
  double histogram[kHistogramSize] = { 1.0 };
  double max_bucket_value = 1.0;

  for (FrameRateCounter::RingBufferType::Iterator it = --fps_counter->end(); it;
       --it) {
    base::TimeDelta delta = fps_counter->RecentFrameInterval(it.index() + 1);

    // Skip this particular instantaneous frame rate if it is not likely to have
    // been valid.
    if (!fps_counter->IsBadFrameInterval(delta)) {
      double fps = 1.0 / delta.InSecondsF();

      // Clamp the FPS to the range we want to plot visually.
      double p = fps / fps_graph_.current_upper_bound;
      if (p > 1.0)
        p = 1.0;

      // Plot this data point.
      SkPoint cur =
          SkPoint::Make(graph_bounds.left() + it.index(),
                        graph_bounds.bottom() - p * graph_bounds.height());
      if (path.isEmpty())
        path.moveTo(cur);
      else
        path.lineTo(cur);

      // Use the fps value to find the right bucket in the histogram.
      int bucket_index = floor(p * (kHistogramSize - 1));

      // Add the delta time to take the time spent at that fps rate into
      // account.
      histogram[bucket_index] += delta.InSecondsF();
      max_bucket_value = std::max(histogram[bucket_index], max_bucket_value);
    }
  }

  // Draw FPS histogram.
  paint.setColor(DebugColors::HUDSeparatorLineColor());
  canvas->drawLine(histogram_bounds.left() - 1,
                   histogram_bounds.top() - 1,
                   histogram_bounds.left() - 1,
                   histogram_bounds.bottom() + 1,
                   paint);
  canvas->drawLine(histogram_bounds.right() + 1,
                   histogram_bounds.top() - 1,
                   histogram_bounds.right() + 1,
                   histogram_bounds.bottom() + 1,
                   paint);

  paint.setColor(DebugColors::FPSDisplayTextAndGraphColor());
  const double bar_height = histogram_bounds.height() / kHistogramSize;

  for (int i = kHistogramSize - 1; i >= 0; --i) {
    if (histogram[i] > 0) {
      double bar_width =
          histogram[i] / max_bucket_value * histogram_bounds.width();
      canvas->drawRect(
          SkRect::MakeXYWH(histogram_bounds.left(),
                           histogram_bounds.bottom() - (i + 1) * bar_height,
                           bar_width,
                           1),
          paint);
    }
  }

  // Draw FPS graph.
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1);
  canvas->drawPath(path, paint);

  return area;
}

SkRect HeadsUpDisplayLayerImpl::DrawMemoryDisplay(SkCanvas* canvas,
                                                  int right,
                                                  int top,
                                                  int width) const {
  if (!memory_entry_.bytes_total())
    return SkRect::MakeEmpty();

  const int kPadding = 4;
  const int kFontHeight = 13;

  const int height = 3 * kFontHeight + 4 * kPadding;
  const int left = bounds().width() - width - right;
  const SkRect area = SkRect::MakeXYWH(left, top, width, height);

  const double megabyte = 1024.0 * 1024.0;

  SkPaint paint = CreatePaint();
  DrawGraphBackground(canvas, &paint, area);

  SkPoint title_pos = SkPoint::Make(left + kPadding, top + kFontHeight);
  SkPoint stat1_pos = SkPoint::Make(left + width - kPadding - 1,
                                    top + kPadding + 2 * kFontHeight);
  SkPoint stat2_pos = SkPoint::Make(left + width - kPadding - 1,
                                    top + 2 * kPadding + 3 * kFontHeight);

  paint.setColor(DebugColors::MemoryDisplayTextColor());
  DrawText(canvas,
           &paint,
           "GPU memory",
           SkPaint::kLeft_Align,
           kFontHeight,
           title_pos);

  std::string text =
      base::StringPrintf("%6.1f MB used",
                         (memory_entry_.bytes_unreleasable +
                          memory_entry_.bytes_allocated) / megabyte);
  DrawText(canvas, &paint, text, SkPaint::kRight_Align, kFontHeight, stat1_pos);

  if (memory_entry_.bytes_over) {
    paint.setColor(SK_ColorRED);
    text = base::StringPrintf("%6.1f MB over",
                              memory_entry_.bytes_over / megabyte);
  } else {
    text = base::StringPrintf("%6.1f MB max ",
                              memory_entry_.total_budget_in_bytes / megabyte);
  }
  DrawText(canvas, &paint, text, SkPaint::kRight_Align, kFontHeight, stat2_pos);

  return area;
}

SkRect HeadsUpDisplayLayerImpl::DrawPaintTimeDisplay(
    SkCanvas* canvas,
    const PaintTimeCounter* paint_time_counter,
    int right,
    int top) const {
  const int kPadding = 4;
  const int kFontHeight = 15;

  const int kGraphWidth = paint_time_counter->HistorySize();
  const int kGraphHeight = 40;

  const int width = kGraphWidth + 2 * kPadding;
  const int height =
      kFontHeight + kGraphHeight + 4 * kPadding + 2 + kFontHeight + kPadding;
  const int left = bounds().width() - width - right;

  const SkRect area = SkRect::MakeXYWH(left, top, width, height);

  SkPaint paint = CreatePaint();
  DrawGraphBackground(canvas, &paint, area);

  SkRect text_bounds = SkRect::MakeXYWH(
      left + kPadding, top + kPadding, kGraphWidth, kFontHeight);
  SkRect text_bounds2 = SkRect::MakeXYWH(left + kPadding,
                                         text_bounds.bottom() + kPadding,
                                         kGraphWidth,
                                         kFontHeight);
  SkRect graph_bounds = SkRect::MakeXYWH(left + kPadding,
                                         text_bounds2.bottom() + 2 * kPadding,
                                         kGraphWidth,
                                         kGraphHeight);

  const std::string value_text =
      base::StringPrintf("%.1f", paint_time_graph_.value);
  const std::string min_max_text = base::StringPrintf(
      "%.1f-%.1f", paint_time_graph_.min, paint_time_graph_.max);

  paint.setColor(DebugColors::PaintTimeDisplayTextAndGraphColor());
  DrawText(canvas,
           &paint,
           "Page paint time (ms)",
           SkPaint::kLeft_Align,
           kFontHeight,
           text_bounds.left(),
           text_bounds.bottom());
  DrawText(canvas,
           &paint,
           value_text,
           SkPaint::kLeft_Align,
           kFontHeight,
           text_bounds2.left(),
           text_bounds2.bottom());
  DrawText(canvas,
           &paint,
           min_max_text,
           SkPaint::kRight_Align,
           kFontHeight,
           text_bounds2.right(),
           text_bounds2.bottom());

  paint.setColor(DebugColors::PaintTimeDisplayTextAndGraphColor());
  for (PaintTimeCounter::RingBufferType::Iterator it =
           paint_time_counter->End();
       it;
       --it) {
    double pt = it->total_time().InMillisecondsF();

    if (pt == 0.0)
      continue;

    double p = pt / paint_time_graph_.current_upper_bound;
    if (p > 1.0)
      p = 1.0;

    canvas->drawRect(
        SkRect::MakeXYWH(graph_bounds.left() + it.index(),
                         graph_bounds.bottom() - p * graph_bounds.height(),
                         1,
                         p * graph_bounds.height()),
        paint);
  }

  DrawGraphLines(canvas, &paint, graph_bounds, paint_time_graph_);

  return area;
}

void HeadsUpDisplayLayerImpl::DrawDebugRects(
    SkCanvas* canvas,
    DebugRectHistory* debug_rect_history) const {
  const std::vector<DebugRect>& debug_rects = debug_rect_history->debug_rects();
  float rect_scale = 1.f / layer_tree_impl()->device_scale_factor();
  SkPaint paint = CreatePaint();

  canvas->save();
  canvas->scale(rect_scale, rect_scale);

  for (size_t i = 0; i < debug_rects.size(); ++i) {
    SkColor stroke_color = 0;
    SkColor fill_color = 0;
    float stroke_width = 0.f;

    switch (debug_rects[i].type) {
      case PAINT_RECT_TYPE:
        stroke_color = DebugColors::PaintRectBorderColor();
        fill_color = DebugColors::PaintRectFillColor();
        stroke_width = DebugColors::PaintRectBorderWidth(layer_tree_impl());
        break;
      case PROPERTY_CHANGED_RECT_TYPE:
        stroke_color = DebugColors::PropertyChangedRectBorderColor();
        fill_color = DebugColors::PropertyChangedRectFillColor();
        stroke_width =
            DebugColors::PropertyChangedRectBorderWidth(layer_tree_impl());
        break;
      case SURFACE_DAMAGE_RECT_TYPE:
        stroke_color = DebugColors::SurfaceDamageRectBorderColor();
        fill_color = DebugColors::SurfaceDamageRectFillColor();
        stroke_width =
            DebugColors::SurfaceDamageRectBorderWidth(layer_tree_impl());
        break;
      case REPLICA_SCREEN_SPACE_RECT_TYPE:
        stroke_color = DebugColors::ScreenSpaceSurfaceReplicaRectBorderColor();
        fill_color = DebugColors::ScreenSpaceSurfaceReplicaRectFillColor();
        stroke_width = DebugColors::ScreenSpaceSurfaceReplicaRectBorderWidth(
            layer_tree_impl());
        break;
      case SCREEN_SPACE_RECT_TYPE:
        stroke_color = DebugColors::ScreenSpaceLayerRectBorderColor();
        fill_color = DebugColors::ScreenSpaceLayerRectFillColor();
        stroke_width =
            DebugColors::ScreenSpaceLayerRectBorderWidth(layer_tree_impl());
        break;
      case OCCLUDING_RECT_TYPE:
        stroke_color = DebugColors::OccludingRectBorderColor();
        fill_color = DebugColors::OccludingRectFillColor();
        stroke_width = DebugColors::OccludingRectBorderWidth(layer_tree_impl());
        break;
      case NONOCCLUDING_RECT_TYPE:
        stroke_color = DebugColors::NonOccludingRectBorderColor();
        fill_color = DebugColors::NonOccludingRectFillColor();
        stroke_width =
            DebugColors::NonOccludingRectBorderWidth(layer_tree_impl());
        break;
    }

    const gfx::RectF& rect = debug_rects[i].rect;
    SkRect sk_rect =
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
    paint.setColor(fill_color);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawRect(sk_rect, paint);

    paint.setColor(stroke_color);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkFloatToScalar(stroke_width));
    canvas->drawRect(sk_rect, paint);
  }

  canvas->restore();
}

const char* HeadsUpDisplayLayerImpl::LayerTypeAsString() const {
  return "HeadsUpDisplayLayer";
}

}  // namespace cc
