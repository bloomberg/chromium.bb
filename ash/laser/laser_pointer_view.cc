// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_view.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <memory>

#include "ash/laser/laser_pointer_points.h"
#include "ash/laser/laser_segment_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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

float DistanceBetweenPoints(const gfx::Point& point1,
                            const gfx::Point& point2) {
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
                                   aura::Window* root_window)
    : laser_points_(life_duration),
      frame_sink_id_(aura::Env::GetInstance()
                         ->context_factory_private()
                         ->AllocateFrameSinkId()),
      frame_sink_support_(this,
                          aura::Env::GetInstance()
                              ->context_factory_private()
                              ->GetSurfaceManager(),
                          frame_sink_id_,
                          false /* is_root */,
                          true /* handles_frame_sink_id_invalidation */,
                          true /* needs_sync_points */),
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

  scale_factor_ = display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(widget_->GetNativeView())
                      .device_scale_factor();
}

LaserPointerView::~LaserPointerView() {
  // Make sure GPU memory buffer is unmapped before being destroyed.
  if (gpu_memory_buffer_)
    gpu_memory_buffer_->Unmap();
}

void LaserPointerView::Stop() {
  buffer_damage_rect_.Union(GetBoundingBox());
  laser_points_.Clear();
  OnPointsUpdated();
}

void LaserPointerView::AddNewPoint(const gfx::Point& new_point) {
  buffer_damage_rect_.Union(GetBoundingBox());
  laser_points_.AddPoint(new_point);
  buffer_damage_rect_.Union(GetBoundingBox());
  OnPointsUpdated();
}

void LaserPointerView::UpdateTime() {
  buffer_damage_rect_.Union(GetBoundingBox());
  // Do not add the point but advance the time if the view is in process of
  // fading away.
  laser_points_.MoveForwardToTime(base::Time::Now());
  buffer_damage_rect_.Union(GetBoundingBox());
  OnPointsUpdated();
}

void LaserPointerView::SetNeedsBeginFrame(bool needs_begin_frame) {
  frame_sink_support_.SetNeedsBeginFrame(needs_begin_frame);
}

void LaserPointerView::SubmitCompositorFrame(
    const cc::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  frame_sink_support_.SubmitCompositorFrame(local_surface_id, std::move(frame));
}

void LaserPointerView::EvictFrame() {
  frame_sink_support_.EvictFrame();
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
  TRACE_EVENT2("ui", "LaserPointerView::UpdatedBuffer", "damage",
               buffer_damage_rect_.ToString(), "points",
               laser_points_.GetNumberOfPoints());

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

    // Map buffer and keep it mapped until destroyed.
    bool rv = gpu_memory_buffer_->Map();
    if (!rv) {
      LOG(ERROR) << "Failed to map GPU memory buffer";
      return;
    }

    // Make sure the first update rectangle covers the whole buffer.
    update_rect = gfx::Rect(screen_bounds.size());
  }

  // Constrain update rectangle to buffer size and early out if empty.
  update_rect.Intersect(gfx::Rect(screen_bounds.size()));
  if (update_rect.IsEmpty())
    return;

  // Create a temporary canvas for update rectangle.
  gfx::Canvas canvas(update_rect.size(), scale_factor_, false);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  // Compute the offset of the current widget.
  gfx::Vector2d widget_offset(
      widget_->GetNativeView()->GetBoundsInRootWindow().origin().x(),
      widget_->GetNativeView()->GetBoundsInRootWindow().origin().y());

  int num_points = laser_points_.GetNumberOfPoints();
  if (num_points) {
    LaserPointerPoints::LaserPoint previous_point = laser_points_.GetOldest();
    previous_point.location -= widget_offset + update_rect.OffsetFromOrigin();
    LaserPointerPoints::LaserPoint current_point;
    std::vector<gfx::PointF> previous_segment_points;
    float previous_radius;
    int current_opacity;

    for (int i = 0; i < num_points; ++i) {
      current_point = laser_points_.laser_points()[i];
      current_point.location -= widget_offset + update_rect.OffsetFromOrigin();

      // Set the radius and opacity based on the distance.
      float current_radius = LinearInterpolate(
          kPointInitialRadius, kPointFinalRadius, current_point.age);
      current_opacity = int{LinearInterpolate(
          kPointInitialOpacity, kPointFinalOpacity, current_point.age)};

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
      flags.setColor(SkColorSetA(kPointColor, current_opacity));
      canvas.DrawPath(path, flags);

      previous_segment_points = current_segment.path_points();
      previous_radius = current_radius;
      previous_point = current_point;
    }

    // Draw the last point as a circle.
    flags.setColor(SkColorSetA(kPointColor, current_opacity));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas.DrawCircle(current_point.location, kPointInitialRadius, flags);
  }

  // Copy result to GPU memory buffer. This is effectiely a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT0("ui", "LaserPointerView::OnPointsUpdated::Copy");

    // Convert update rectangle to pixel coordinates.
    gfx::Rect pixel_rect =
        gfx::ScaleToEnclosingRect(update_rect, scale_factor_);
    uint8_t* data = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    int stride = gpu_memory_buffer_->stride(0);
    canvas.sk_canvas()->readPixels(
        SkImageInfo::MakeN32Premul(pixel_rect.width(), pixel_rect.height()),
        data + pixel_rect.y() * stride + pixel_rect.x() * 4, stride, 0, 0);
  }

  // Update surface damage rectangle.
  surface_damage_rect_.Union(update_rect);

  needs_update_surface_ = true;

  // Early out if waiting for last surface update to be drawn.
  if (pending_draw_surface_)
    return;

  UpdateSurface();
}

void LaserPointerView::UpdateSurface() {
  TRACE_EVENT1("ui", "LaserPointerView::UpdatedSurface", "damage",
               surface_damage_rect_.ToString());

  DCHECK(needs_update_surface_);
  needs_update_surface_ = false;

  std::unique_ptr<LaserResource> resource;
  // Reuse returned resource if available.
  if (!returned_resources_.empty()) {
    resource = std::move(returned_resources_.front());
    returned_resources_.pop_front();
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
  quad_state->quad_layer_bounds = quad_rect.size();
  quad_state->visible_quad_layer_rect = quad_rect;
  quad_state->opacity = 1.0f;

  cc::CompositorFrame frame;
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

  // Set layer surface if this is the initial frame.
  if (!local_surface_id_.is_valid()) {
    local_surface_id_ = id_allocator_.GenerateId();
    widget_->GetNativeView()->layer()->SetShowPrimarySurface(
        cc::SurfaceInfo(cc::SurfaceId(frame_sink_id_, local_surface_id_), 1.0f,
                        quad_rect.size()),
        aura::Env::GetInstance()
            ->context_factory_private()
            ->GetSurfaceManager()
            ->reference_factory());
    widget_->GetNativeView()->layer()->SetFillsBoundsOpaquely(false);
  }

  SubmitCompositorFrame(local_surface_id_, std::move(frame));

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
