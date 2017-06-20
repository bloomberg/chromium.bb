// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_view.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

#include "ash/laser/laser_pointer_points.h"
#include "ash/laser/laser_segment_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/resources/transferable_resource.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/layout.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Variables for rendering the laser. Radius in DIP.
const float kPointInitialRadius = 5.0f;
const float kPointFinalRadius = 0.25f;
const int kPointInitialOpacity = 200;
const int kPointFinalOpacity = 10;
const SkColor kPointColor = SkColorSetRGB(255, 0, 0);
// Change this when debugging prediction code.
const SkColor kPredictionPointColor = kPointColor;

float DistanceBetweenPoints(const gfx::PointF& point1,
                            const gfx::PointF& point2) {
  return (point1 - point2).Length();
}

float LinearInterpolate(float initial_value,
                        float final_value,
                        float progress) {
  return initial_value + (final_value - initial_value) * progress;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// The laser segment calcuates the path needed to draw a laser segment. A laser
// segment is used instead of just a regular line segments to avoid overlapping.
// A laser segment looks as follows:
//    _______         _________       _________        _________
//   /       \        \       /      /         /      /         \       |
//   |   A   |       2|.  B  .|1    2|.   C   .|1    2|.   D     \.1    |
//   |       |        |       |      |         |      |          /      |
//    \_____/         /_______\      \_________\      \_________/       |
//
//
// Given a start and end point (represented by the periods in the above
// diagrams), we create each segment by projecting each point along the normal
// to the line segment formed by the start(1) and end(2) points. We then
// create a path using arcs and lines. There are three types of laser segments:
// head(B), regular(C) and tail(D). A typical laser is created by rendering one
// tail(D), zero or more regular segments(C), one head(B) and a circle at the
// end(A). They are meant to fit perfectly with the previous and next segments,
// so that no whitespace/overlap is shown.
// A more detailed version of this is located at https://goo.gl/qixdux.
class LaserSegment {
 public:
  LaserSegment(const std::vector<gfx::PointF>& previous_points,
               const gfx::PointF& start_point,
               const gfx::PointF& end_point,
               float start_radius,
               float end_radius,
               bool is_last_segment) {
    DCHECK(previous_points.empty() || previous_points.size() == 2u);
    bool is_first_segment = previous_points.empty();

    // Calculate the variables for the equation of the lines which pass through
    // the start and end points, and are perpendicular to the line segment
    // between the start and end points.
    float slope, start_y_intercept, end_y_intercept;
    ComputeNormalLineVariables(start_point, end_point, &slope,
                               &start_y_intercept, &end_y_intercept);

    // Project the points along normal line by the given radius.
    gfx::PointF end_first_projection, end_second_projection;
    ComputeProjectedPoints(end_point, slope, end_y_intercept, end_radius,
                           &end_first_projection, &end_second_projection);

    // Create a collection of the points used to create the path and reorder
    // them as needed.
    std::vector<gfx::PointF> ordered_points;
    ordered_points.reserve(4);
    if (!is_first_segment) {
      ordered_points.push_back(previous_points[1]);
      ordered_points.push_back(previous_points[0]);
    } else {
      // We push two of the same point, so that for both cases we have 4 points,
      // and we can use the same indexes when creating the path.
      ordered_points.push_back(start_point);
      ordered_points.push_back(start_point);
    }
    // Push the projected points so that the the smaller angle relative to the
    // line segment between the two data points is first. This will ensure there
    // is always a anticlockwise arc between the last two points, and always a
    // clockwise arc for these two points if and when they are used in the next
    // segment.
    if (IsFirstPointSmallerAngle(start_point, end_point, end_first_projection,
                                 end_second_projection)) {
      ordered_points.push_back(end_first_projection);
      ordered_points.push_back(end_second_projection);
    } else {
      ordered_points.push_back(end_second_projection);
      ordered_points.push_back(end_first_projection);
    }

    // Create the path. The path always goes as follows:
    // 1. Move to point 0.
    // 2. Arc clockwise from point 0 to point 1. This step is skipped if it
    //    is the tail segment.
    // 3. Line from point 1 to point 2.
    // 4. Arc anticlockwise from point 2 to point 3. Arc clockwise if this is
    //    the head segment.
    // 5. Line from point 3 to point 0.
    //      2           1
    //       *---------*                   |
    //      /         /                    |
    //      |         |                    |
    //      |         |                    |
    //      \         \                    |
    //       *--------*
    //      3          0
    DCHECK_EQ(4u, ordered_points.size());
    path_.moveTo(ordered_points[0].x(), ordered_points[0].y());
    if (!is_first_segment) {
      path_.arcTo(start_radius, start_radius, 180.0f, gfx::Path::kSmall_ArcSize,
                  gfx::Path::kCW_Direction, ordered_points[1].x(),
                  ordered_points[1].y());
    }

    path_.lineTo(ordered_points[2].x(), ordered_points[2].y());
    path_.arcTo(
        end_radius, end_radius, 180.0f, gfx::Path::kSmall_ArcSize,
        is_last_segment ? gfx::Path::kCW_Direction : gfx::Path::kCCW_Direction,
        ordered_points[3].x(), ordered_points[3].y());
    path_.lineTo(ordered_points[0].x(), ordered_points[0].y());

    // Store data to be used by the next segment.
    path_points_.push_back(ordered_points[2]);
    path_points_.push_back(ordered_points[3]);
  }

  SkPath path() const { return path_; }
  std::vector<gfx::PointF> path_points() const { return path_points_; }

 private:
  SkPath path_;
  std::vector<gfx::PointF> path_points_;

  DISALLOW_COPY_AND_ASSIGN(LaserSegment);
};

class LaserLayerTreeFrameSinkHolder : public cc::LayerTreeFrameSinkClient {
 public:
  LaserLayerTreeFrameSinkHolder(
      LaserPointerView* view,
      std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
      : view_(view), frame_sink_(std::move(frame_sink)) {
    frame_sink_->BindToClient(this);
  }
  ~LaserLayerTreeFrameSinkHolder() override { frame_sink_->DetachFromClient(); }

  cc::LayerTreeFrameSink* frame_sink() { return frame_sink_.get(); }

  // Called before laser pointer view is destroyed.
  void OnLaserPointerViewDestroying() { view_ = nullptr; }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(cc::BeginFrameSource* source) override {}
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override {
    if (view_)
      view_->ReclaimResources(resources);
  }
  void SetTreeActivationCallback(const base::Closure& callback) override {}
  void DidReceiveCompositorFrameAck() override {
    if (view_)
      view_->DidReceiveCompositorFrameAck();
  }
  void DidLoseLayerTreeFrameSink() override {}
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

 private:
  LaserPointerView* view_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;

  DISALLOW_COPY_AND_ASSIGN(LaserLayerTreeFrameSinkHolder);
};

// This struct contains the resources associated with a laser pointer frame.
struct LaserResource {
  LaserResource() {}
  ~LaserResource() {
    if (context_provider) {
      gpu::gles2::GLES2Interface* gles2 = context_provider->ContextGL();
      if (texture)
        gles2->DeleteTextures(1, &texture);
      if (image)
        gles2->DestroyImageCHROMIUM(image);
    }
  }
  scoped_refptr<cc::ContextProvider> context_provider;
  uint32_t texture = 0;
  uint32_t image = 0;
  gpu::Mailbox mailbox;
};

// LaserPointerView
LaserPointerView::LaserPointerView(base::TimeDelta life_duration,
                                   base::TimeDelta presentation_delay,
                                   aura::Window* root_window)
    : laser_points_(life_duration),
      predicted_laser_points_(life_duration),
      presentation_delay_(presentation_delay),
      weak_ptr_factory_(this) {
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "LaserOverlay";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  params.layer_type = ui::LAYER_SOLID_COLOR;

  widget_->Init(params);
  widget_->Show();
  widget_->SetContentsView(this);
  widget_->SetBounds(root_window->GetBoundsInScreen());
  set_owned_by_client();

  scale_factor_ = ui::GetScaleFactorForNativeView(widget_->GetNativeView());

  frame_sink_holder_ = base::MakeUnique<LaserLayerTreeFrameSinkHolder>(
      this, widget_->GetNativeView()->CreateLayerTreeFrameSink());
}

LaserPointerView::~LaserPointerView() {
  frame_sink_holder_->OnLaserPointerViewDestroying();
}

void LaserPointerView::Stop() {
  buffer_damage_rect_.Union(GetBoundingBox());
  laser_points_.Clear();
  predicted_laser_points_.Clear();
  OnPointsUpdated();
}

void LaserPointerView::AddNewPoint(const gfx::PointF& new_point,
                                   const base::TimeTicks& new_time) {
  TRACE_EVENT1("ui", "LaserPointerView::AddNewPoint", "new_point",
               new_point.ToString());
  TRACE_COUNTER1(
      "ui", "LaserPointerPredictionError",
      predicted_laser_points_.GetNumberOfPoints()
          ? std::round((new_point -
                        predicted_laser_points_.laser_points().front().location)
                           .Length())
          : 0);

  buffer_damage_rect_.Union(GetBoundingBox());
  laser_points_.AddPoint(new_point, new_time);

  // Current time is needed to determine presentation time and the number of
  // predicted points to add.
  base::TimeTicks current_time = ui::EventTimeForNow();

  // Create a new set of predicted points based on the last four points added.
  // We add enough predicted points to fill the time between the new point and
  // the expected presentation time. Note that estimated presentation time is
  // based on current time and inefficient rendering of points can result in an
  // actual presentation time that is later.
  predicted_laser_points_.Clear();

  // Normalize all coordinates to screen size.
  gfx::Size screen_size = widget_->GetNativeView()->GetBoundsInScreen().size();
  gfx::Vector2dF scale(1.0f / screen_size.width(), 1.0f / screen_size.height());

  // TODO(reveman): Determine interval based on history when event time stamps
  // are accurate. b/36137953
  const float kPredictionIntervalMs = 5.0f;
  const float kMaxPointIntervalMs = 10.0f;
  base::TimeDelta prediction_interval =
      base::TimeDelta::FromMilliseconds(kPredictionIntervalMs);
  base::TimeDelta max_point_interval =
      base::TimeDelta::FromMilliseconds(kMaxPointIntervalMs);
  base::TimeTicks last_point_time = new_time;
  gfx::PointF last_point_location =
      gfx::ScalePoint(new_point, scale.x(), scale.y());

  // Use the last four points for prediction.
  using PositionArray = std::array<gfx::PointF, 4>;
  PositionArray position;
  PositionArray::iterator it = position.begin();
  for (const auto& point : base::Reversed(laser_points_.laser_points())) {
    // Stop adding positions if interval between points is too large to provide
    // an accurate history for prediction.
    if ((last_point_time - point.time) > max_point_interval)
      break;

    last_point_time = point.time;
    last_point_location = gfx::ScalePoint(point.location, scale.x(), scale.y());
    *it++ = last_point_location;

    // Stop when no more positions are needed.
    if (it == position.end())
      break;
  }
  // Pad with last point if needed.
  std::fill(it, position.end(), last_point_location);

  // Note: Currently there's no need to divide by the time delta between
  // points as we assume a constant delta between points that matches the
  // prediction point interval.
  gfx::Vector2dF velocity[3];
  for (size_t i = 0; i < arraysize(velocity); ++i)
    velocity[i] = position[i] - position[i + 1];

  gfx::Vector2dF acceleration[2];
  for (size_t i = 0; i < arraysize(acceleration); ++i)
    acceleration[i] = velocity[i] - velocity[i + 1];

  gfx::Vector2dF jerk = acceleration[0] - acceleration[1];

  // Adjust max prediction time based on speed as prediction data is not great
  // at lower speeds.
  const float kMaxPredictionScaleSpeed = 1e-5;
  double speed = velocity[0].LengthSquared();
  base::TimeTicks max_prediction_time =
      current_time +
      std::min(presentation_delay_ * (speed / kMaxPredictionScaleSpeed),
               presentation_delay_);

  // Add predicted points until we reach the max prediction time.
  gfx::PointF location = position[0];
  for (base::TimeTicks time = new_time + prediction_interval;
       time < max_prediction_time; time += prediction_interval) {
    // Note: Currently there's no need to multiply by the prediction interval
    // as the velocity is calculated based on a time delta between points that
    // is the same as the prediction interval.
    velocity[0] += acceleration[0];
    acceleration[0] += jerk;
    location += velocity[0];

    predicted_laser_points_.AddPoint(
        gfx::ScalePoint(location, screen_size.width(), screen_size.height()),
        time);

    // Always stop at three predicted points as a four point history doesn't
    // provide accurate prediction of more points.
    if (predicted_laser_points_.GetNumberOfPoints() == 3)
      break;
  }

  // Move forward to next presentation time.
  base::TimeTicks next_presentation_time = current_time + presentation_delay_;
  laser_points_.MoveForwardToTime(next_presentation_time);
  predicted_laser_points_.MoveForwardToTime(next_presentation_time);

  buffer_damage_rect_.Union(GetBoundingBox());
  OnPointsUpdated();
}

void LaserPointerView::UpdateTime() {
  buffer_damage_rect_.Union(GetBoundingBox());
  // Do not add the point but advance the time if the view is in process of
  // fading away.
  base::TimeTicks next_presentation_time =
      ui::EventTimeForNow() + presentation_delay_;
  laser_points_.MoveForwardToTime(next_presentation_time);
  predicted_laser_points_.MoveForwardToTime(next_presentation_time);
  buffer_damage_rect_.Union(GetBoundingBox());
  OnPointsUpdated();
}

void LaserPointerView::DidReceiveCompositorFrameAck() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&LaserPointerView::OnDidDrawSurface,
                            weak_ptr_factory_.GetWeakPtr()));
}

void LaserPointerView::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK_EQ(resources.size(), 1u);

  auto it = resources_.find(resources.front().id);
  DCHECK(it != resources_.end());
  std::unique_ptr<LaserResource> resource = std::move(it->second);
  resources_.erase(it);

  gpu::gles2::GLES2Interface* gles2 = resource->context_provider->ContextGL();
  if (resources.front().sync_token.HasData())
    gles2->WaitSyncTokenCHROMIUM(resources.front().sync_token.GetConstData());

  if (!resources.front().lost)
    returned_resources_.push_back(std::move(resource));
}

gfx::Rect LaserPointerView::GetBoundingBox() {
  // Expand the bounding box so that it includes the radius of the points on the
  // edges and antialiasing.
  gfx::Rect bounding_box = laser_points_.GetBoundingBox();
  bounding_box.Union(predicted_laser_points_.GetBoundingBox());
  const int kOutsetForAntialiasing = 1;
  int outset = kPointInitialRadius + kOutsetForAntialiasing;
  bounding_box.Inset(-outset, -outset);
  return bounding_box;
}

void LaserPointerView::OnPointsUpdated() {
  if (pending_update_buffer_)
    return;

  pending_update_buffer_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&LaserPointerView::UpdateBuffer,
                            weak_ptr_factory_.GetWeakPtr()));
}

void LaserPointerView::UpdateBuffer() {
  TRACE_EVENT1("ui", "LaserPointerView::UpdateBuffer", "damage",
               buffer_damage_rect_.ToString());

  DCHECK(pending_update_buffer_);
  pending_update_buffer_ = false;

  gfx::Rect screen_bounds = widget_->GetNativeView()->GetBoundsInScreen();
  gfx::Rect update_rect = buffer_damage_rect_;
  buffer_damage_rect_ = gfx::Rect();

  // Create and map a single GPU memory buffer. The laser pointer will be
  // written into this buffer without any buffering. The result is that we
  // might be modifying the buffer while it's being displayed. This provides
  // minimal latency but potential tearing. Note that we have to draw into
  // a temporary surface and copy it into GPU memory buffer to avoid flicker.
  if (!gpu_memory_buffer_) {
    TRACE_EVENT0("ui", "LaserPointerView::UpdateBuffer::Create");

    gpu_memory_buffer_ =
        aura::Env::GetInstance()
            ->context_factory()
            ->GetGpuMemoryBufferManager()
            ->CreateGpuMemoryBuffer(
                gfx::ScaleToCeiledSize(screen_bounds.size(), scale_factor_),
                SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                             : gfx::BufferFormat::BGRA_8888,
                gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                gpu::kNullSurfaceHandle);
    if (!gpu_memory_buffer_) {
      LOG(ERROR) << "Failed to allocate GPU memory buffer";
      return;
    }

    // Make sure the first update rectangle covers the whole buffer.
    update_rect = gfx::Rect(screen_bounds.size());
  }

  // Constrain update rectangle to buffer size and early out if empty.
  update_rect.Intersect(gfx::Rect(screen_bounds.size()));
  if (update_rect.IsEmpty())
    return;

  // Map buffer for writing.
  if (!gpu_memory_buffer_->Map()) {
    LOG(ERROR) << "Failed to map GPU memory buffer";
    return;
  }

  // Create a temporary canvas for update rectangle.
  gfx::Canvas canvas(update_rect.size(), scale_factor_, false);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  // Compute the offset of the current widget.
  gfx::Vector2d widget_offset(
      widget_->GetNativeView()->GetBoundsInRootWindow().origin().x(),
      widget_->GetNativeView()->GetBoundsInRootWindow().origin().y());

  int num_points = laser_points_.GetNumberOfPoints() +
                   predicted_laser_points_.GetNumberOfPoints();
  if (num_points) {
    TRACE_EVENT1("ui", "LaserPointerView::UpdateBuffer::Paint", "update_rect",
                 update_rect.ToString());

    LaserPointerPoints::LaserPoint previous_point = laser_points_.GetOldest();
    previous_point.location -= widget_offset + update_rect.OffsetFromOrigin();
    LaserPointerPoints::LaserPoint current_point;
    std::vector<gfx::PointF> previous_segment_points;
    float previous_radius;
    int current_opacity;

    for (int i = 0; i < num_points; ++i) {
      if (i < laser_points_.GetNumberOfPoints()) {
        current_point = laser_points_.laser_points()[i];
      } else {
        current_point =
            predicted_laser_points_
                .laser_points()[i - laser_points_.GetNumberOfPoints()];
      }
      current_point.location -= widget_offset + update_rect.OffsetFromOrigin();

      // Set the radius and opacity based on the distance.
      float current_radius = LinearInterpolate(
          kPointInitialRadius, kPointFinalRadius, current_point.age);
      current_opacity = static_cast<int>(LinearInterpolate(
          kPointInitialOpacity, kPointFinalOpacity, current_point.age));

      // If we draw laser_points_ that are within a stroke width of each other,
      // the result will be very jagged, unless we are on the last point, then
      // we draw regardless.
      float distance_threshold = current_radius * 2.0f;
      if (DistanceBetweenPoints(previous_point.location,
                                current_point.location) <= distance_threshold &&
          i != num_points - 1) {
        continue;
      }

      LaserSegment current_segment(
          previous_segment_points, gfx::PointF(previous_point.location),
          gfx::PointF(current_point.location), previous_radius, current_radius,
          i == num_points - 1);

      SkPath path = current_segment.path();
      if (i < laser_points_.GetNumberOfPoints())
        flags.setColor(SkColorSetA(kPointColor, current_opacity));
      else
        flags.setColor(SkColorSetA(kPredictionPointColor, current_opacity));
      canvas.DrawPath(path, flags);

      previous_segment_points = current_segment.path_points();
      previous_radius = current_radius;
      previous_point = current_point;
    }

    // Draw the last point as a circle.
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas.DrawCircle(current_point.location, kPointInitialRadius, flags);
  }

  // Convert update rectangle to pixel coordinates.
  gfx::Rect pixel_rect = gfx::ScaleToEnclosingRect(update_rect, scale_factor_);

  // Copy result to GPU memory buffer. This is effectiely a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT1("ui", "LaserPointerView::UpdateBuffer::Copy", "pixel_rect",
                 pixel_rect.ToString());

    uint8_t* data = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    int stride = gpu_memory_buffer_->stride(0);
    canvas.GetBitmap().readPixels(
        SkImageInfo::MakeN32Premul(pixel_rect.width(), pixel_rect.height()),
        data + pixel_rect.y() * stride + pixel_rect.x() * 4, stride, 0, 0);
  }

  // Unmap to flush writes to buffer.
  gpu_memory_buffer_->Unmap();

  // Update surface damage rectangle.
  surface_damage_rect_.Union(update_rect);

  needs_update_surface_ = true;

  // Early out if waiting for last surface update to be drawn.
  if (pending_draw_surface_)
    return;

  UpdateSurface();
}

void LaserPointerView::UpdateSurface() {
  TRACE_EVENT1("ui", "LaserPointerView::UpdateSurface", "damage",
               surface_damage_rect_.ToString());

  DCHECK(needs_update_surface_);
  needs_update_surface_ = false;

  std::unique_ptr<LaserResource> resource;
  // Reuse returned resource if available.
  if (!returned_resources_.empty()) {
    resource = std::move(returned_resources_.back());
    returned_resources_.pop_back();
  }

  // Create new resource if needed.
  if (!resource)
    resource = base::MakeUnique<LaserResource>();

  // Acquire context provider for resource if needed.
  // Note: We make no attempts to recover if the context provider is later
  // lost. It is expected that this class is short-lived and requiring a
  // new instance to be created in lost context situations is acceptable and
  // keeps the code simple.
  if (!resource->context_provider) {
    resource->context_provider = aura::Env::GetInstance()
                                     ->context_factory()
                                     ->SharedMainThreadContextProvider();
    if (!resource->context_provider) {
      LOG(ERROR) << "Failed to acquire a context provider";
      return;
    }
  }

  gpu::gles2::GLES2Interface* gles2 = resource->context_provider->ContextGL();

  if (resource->texture) {
    gles2->ActiveTexture(GL_TEXTURE0);
    gles2->BindTexture(GL_TEXTURE_2D, resource->texture);
  } else {
    gles2->GenTextures(1, &resource->texture);
    gles2->ActiveTexture(GL_TEXTURE0);
    gles2->BindTexture(GL_TEXTURE_2D, resource->texture);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gles2->GenMailboxCHROMIUM(resource->mailbox.name);
    gles2->ProduceTextureCHROMIUM(GL_TEXTURE_2D, resource->mailbox.name);
  }

  gfx::Size buffer_size = gpu_memory_buffer_->GetSize();

  if (resource->image) {
    gles2->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, resource->image);
  } else {
    resource->image = gles2->CreateImageCHROMIUM(
        gpu_memory_buffer_->AsClientBuffer(), buffer_size.width(),
        buffer_size.height(), SK_B32_SHIFT ? GL_RGBA : GL_BGRA_EXT);
    if (!resource->image) {
      LOG(ERROR) << "Failed to create image";
      return;
    }
  }
  gles2->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, resource->image);

  gpu::SyncToken sync_token;
  uint64_t fence_sync = gles2->InsertFenceSyncCHROMIUM();
  gles2->OrderingBarrierCHROMIUM();
  gles2->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  cc::TransferableResource transferable_resource;
  transferable_resource.id = next_resource_id_++;
  transferable_resource.format = cc::RGBA_8888;
  transferable_resource.filter = GL_LINEAR;
  transferable_resource.size = buffer_size;
  transferable_resource.mailbox_holder =
      gpu::MailboxHolder(resource->mailbox, sync_token, GL_TEXTURE_2D);
  transferable_resource.is_overlay_candidate = true;

  gfx::Rect quad_rect(widget_->GetNativeView()->GetBoundsInScreen().size());

  const int kRenderPassId = 1;
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(kRenderPassId, quad_rect, surface_damage_rect_,
                      gfx::Transform());
  surface_damage_rect_ = gfx::Rect();

  cc::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->quad_layer_rect = quad_rect;
  quad_state->visible_quad_layer_rect = quad_rect;
  quad_state->opacity = 1.0f;

  cc::CompositorFrame frame;
  // TODO(eseckler): LaserPointerView should use BeginFrames and set the ack
  // accordingly.
  frame.metadata.begin_frame_ack =
      cc::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor =
      widget_->GetLayer()->device_scale_factor();
  cc::TextureDrawQuad* texture_quad =
      render_pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  texture_quad->SetNew(quad_state, quad_rect, gfx::Rect(), quad_rect,
                       transferable_resource.id, true, uv_top_left,
                       uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
                       false, false, false);
  texture_quad->set_resource_size_in_pixels(transferable_resource.size);
  frame.resource_list.push_back(transferable_resource);
  frame.render_pass_list.push_back(std::move(render_pass));

  frame_sink_holder_->frame_sink()->SubmitCompositorFrame(std::move(frame));

  resources_[transferable_resource.id] = std::move(resource);

  DCHECK(!pending_draw_surface_);
  pending_draw_surface_ = true;
}

void LaserPointerView::OnDidDrawSurface() {
  pending_draw_surface_ = false;
  if (needs_update_surface_)
    UpdateSurface();
}

}  // namespace ash
