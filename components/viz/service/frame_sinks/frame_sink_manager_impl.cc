// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display_embedder/display_provider.h"
#include "components/viz/service/display_embedder/external_begin_frame_controller_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/primary_begin_frame_source.h"
#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace viz {

FrameSinkManagerImpl::FrameSinkSourceMapping::FrameSinkSourceMapping() =
    default;

FrameSinkManagerImpl::FrameSinkSourceMapping::FrameSinkSourceMapping(
    FrameSinkSourceMapping&& other) = default;

FrameSinkManagerImpl::FrameSinkSourceMapping::~FrameSinkSourceMapping() =
    default;

FrameSinkManagerImpl::FrameSinkSourceMapping&
FrameSinkManagerImpl::FrameSinkSourceMapping::operator=(
    FrameSinkSourceMapping&& other) = default;

FrameSinkManagerImpl::SinkAndSupport::SinkAndSupport() = default;

FrameSinkManagerImpl::SinkAndSupport::SinkAndSupport(SinkAndSupport&& other) =
    default;

FrameSinkManagerImpl::SinkAndSupport::~SinkAndSupport() = default;

FrameSinkManagerImpl::SinkAndSupport& FrameSinkManagerImpl::SinkAndSupport::
operator=(SinkAndSupport&& other) = default;

FrameSinkManagerImpl::FrameSinkManagerImpl(
    SurfaceManager::LifetimeType lifetime_type,
    uint32_t number_of_frames_to_activation_deadline,
    DisplayProvider* display_provider)
    : display_provider_(display_provider),
      surface_manager_(lifetime_type, number_of_frames_to_activation_deadline),
      hit_test_manager_(this),
      binding_(this) {
  surface_manager_.AddObserver(&hit_test_manager_);
  surface_manager_.AddObserver(this);
}

FrameSinkManagerImpl::~FrameSinkManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_capturers_.clear();
  // All FrameSinks should be unregistered prior to FrameSinkManager
  // destruction.
  compositor_frame_sinks_.clear();
  DCHECK_EQ(registered_sources_.size(), 0u);
  surface_manager_.RemoveObserver(this);
  surface_manager_.RemoveObserver(&hit_test_manager_);
}

void FrameSinkManagerImpl::BindAndSetClient(
    mojom::FrameSinkManagerRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::FrameSinkManagerClientPtr client) {
  DCHECK(!client_);
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request), std::move(task_runner));
  client_ptr_ = std::move(client);

  client_ = client_ptr_.get();
}

void FrameSinkManagerImpl::SetLocalClient(
    mojom::FrameSinkManagerClient* client) {
  DCHECK(!client_ptr_);

  client_ = client;
}

void FrameSinkManagerImpl::RegisterFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  surface_manager_.RegisterFrameSinkId(frame_sink_id);
  if (video_detector_)
    video_detector_->OnFrameSinkIdRegistered(frame_sink_id);
}

void FrameSinkManagerImpl::InvalidateFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  surface_manager_.InvalidateFrameSinkId(frame_sink_id);
  if (video_detector_)
    video_detector_->OnFrameSinkIdInvalidated(frame_sink_id);

  // Destroy the [Root]CompositorFrameSinkImpl if there is one. This will result
  // in UnregisterCompositorFrameSinkSupport() being called and |iter| will be
  // invalidated afterwards
  auto iter = compositor_frame_sinks_.find(frame_sink_id);
  if (iter != compositor_frame_sinks_.end())
    iter->second.sink.reset();
}

void FrameSinkManagerImpl::SetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id,
    const std::string& debug_label) {
  surface_manager_.SetFrameSinkDebugLabel(frame_sink_id, debug_label);
}

void FrameSinkManagerImpl::CreateRootCompositorFrameSink(
    mojom::RootCompositorFrameSinkParamsPtr params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(params->frame_sink_id));
  DCHECK(display_provider_);

  std::unique_ptr<ExternalBeginFrameControllerImpl>
      external_begin_frame_controller;
  if (params->external_begin_frame_controller.is_pending() &&
      params->external_begin_frame_controller_client) {
    external_begin_frame_controller =
        std::make_unique<ExternalBeginFrameControllerImpl>(
            std::move(params->external_begin_frame_controller),
            mojom::ExternalBeginFrameControllerClientPtr(
                std::move(params->external_begin_frame_controller_client)));
  }

  std::unique_ptr<SyntheticBeginFrameSource> begin_frame_source;
  auto display = display_provider_->CreateDisplay(
      params->frame_sink_id, params->widget, params->force_software_compositing,
      external_begin_frame_controller.get(), params->renderer_settings,
      &begin_frame_source);

  auto frame_sink = std::make_unique<RootCompositorFrameSinkImpl>(
      this, params->frame_sink_id, std::move(display),
      std::move(begin_frame_source), std::move(external_begin_frame_controller),
      std::move(params->compositor_frame_sink),
      mojom::CompositorFrameSinkClientPtr(
          std::move(params->compositor_frame_sink_client)),
      std::move(params->display_private),
      mojom::DisplayClientPtr(std::move(params->display_client)));
  SinkAndSupport& entry = compositor_frame_sinks_[params->frame_sink_id];
  DCHECK(entry.support);  // |entry| was created by RootCompositorFrameSinkImpl.
  entry.sink = std::move(frame_sink);
}

void FrameSinkManagerImpl::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    mojom::CompositorFrameSinkRequest request,
    mojom::CompositorFrameSinkClientPtr client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  auto frame_sink = std::make_unique<CompositorFrameSinkImpl>(
      this, frame_sink_id, std::move(request), std::move(client));
  SinkAndSupport& entry = compositor_frame_sinks_[frame_sink_id];
  DCHECK(entry.support);  // |entry| was created by CompositorFrameSinkImpl.
  entry.sink = std::move(frame_sink);
}

void FrameSinkManagerImpl::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // If it's possible to reach the parent through the child's descendant chain,
  // then this will create an infinite loop.  Might as well just crash here.
  CHECK(!ChildContains(child_frame_sink_id, parent_frame_sink_id));

  auto& children = frame_sink_source_map_[parent_frame_sink_id].children;
  DCHECK(!base::ContainsKey(children, child_frame_sink_id));
  children.insert(child_frame_sink_id);

  // If the parent has no source, then attaching it to this child will
  // not change any downstream sources.
  BeginFrameSource* parent_source =
      frame_sink_source_map_[parent_frame_sink_id].source;
  if (!parent_source)
    return;

  DCHECK_EQ(registered_sources_.count(parent_source), 1u);
  RecursivelyAttachBeginFrameSource(child_frame_sink_id, parent_source);
}

void FrameSinkManagerImpl::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Deliberately do not check validity of either parent or child FrameSinkId
  // here. They were valid during the registration, so were valid at some point
  // in time. This makes it possible to invalidate parent and child FrameSinkIds
  // independently of each other and not have an ordering dependency of
  // unregistering the hierarchy first before either of them.
  auto iter = frame_sink_source_map_.find(parent_frame_sink_id);
  DCHECK(iter != frame_sink_source_map_.end());

  // Remove |child_frame_sink_id| from parents list of children.
  auto& mapping = iter->second;
  DCHECK(base::ContainsKey(mapping.children, child_frame_sink_id));
  mapping.children.erase(child_frame_sink_id);

  // Delete the FrameSinkSourceMapping for |parent_frame_sink_id| if empty.
  if (!mapping.has_children() && !mapping.source) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter->second.source;
  if (!parent_source)
    return;

  // TODO(enne): these walks could be done in one step.
  RecursivelyDetachBeginFrameSource(child_frame_sink_id, parent_source);
  for (auto& source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

void FrameSinkManagerImpl::AssignTemporaryReference(const SurfaceId& surface_id,
                                                    const FrameSinkId& owner) {
  surface_manager_.AssignTemporaryReference(surface_id, owner);
}

void FrameSinkManagerImpl::DropTemporaryReference(const SurfaceId& surface_id) {
  surface_manager_.DropTemporaryReference(surface_id);
}

void FrameSinkManagerImpl::AddVideoDetectorObserver(
    mojom::VideoDetectorObserverPtr observer) {
  if (!video_detector_)
    video_detector_ = std::make_unique<VideoDetector>(&surface_manager_);
  video_detector_->AddObserver(std::move(observer));
}

void FrameSinkManagerImpl::CreateVideoCapturer(
    mojom::FrameSinkVideoCapturerRequest request) {
  video_capturers_.emplace(
      std::make_unique<FrameSinkVideoCapturerImpl>(this, std::move(request)));
}

void FrameSinkManagerImpl::RegisterCompositorFrameSinkSupport(
    const FrameSinkId& frame_sink_id,
    CompositorFrameSinkSupport* support) {
  DCHECK(support);

  SinkAndSupport& entry = compositor_frame_sinks_[frame_sink_id];
  DCHECK(!entry.support);
  entry.support = support;

  for (auto& capturer : video_capturers_) {
    if (capturer->requested_target() == frame_sink_id)
      capturer->SetResolvedTarget(entry.support);
  }

  auto it = frame_sink_source_map_.find(frame_sink_id);
  if (it != frame_sink_source_map_.end() && it->second.source)
    support->SetBeginFrameSource(it->second.source);
}

void FrameSinkManagerImpl::UnregisterCompositorFrameSinkSupport(
    const FrameSinkId& frame_sink_id) {
  DCHECK_EQ(compositor_frame_sinks_.count(frame_sink_id), 1u);

  for (auto& capturer : video_capturers_) {
    if (capturer->requested_target() == frame_sink_id)
      capturer->OnTargetWillGoAway();
  }

  compositor_frame_sinks_.erase(frame_sink_id);
}

void FrameSinkManagerImpl::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);

  primary_source_.OnBeginFrameSourceAdded(source);
}

void FrameSinkManagerImpl::UnregisterBeginFrameSource(
    BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

  primary_source_.OnBeginFrameSourceRemoved(source);

  if (frame_sink_source_map_.count(frame_sink_id) == 0u)
    return;

  // TODO(enne): these walks could be done in one step.
  // Remove this begin frame source from its subtree.
  RecursivelyDetachBeginFrameSource(frame_sink_id, source);
  // Then flush every remaining registered source to fix any sources that
  // became null because of the previous step but that have an alternative.
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

BeginFrameSource* FrameSinkManagerImpl::GetPrimaryBeginFrameSource() {
  return &primary_source_;
}

void FrameSinkManagerImpl::RecursivelyAttachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[frame_sink_id];
  if (!mapping.source) {
    mapping.source = source;
    auto iter = compositor_frame_sinks_.find(frame_sink_id);
    if (iter != compositor_frame_sinks_.end())
      iter->second.support->SetBeginFrameSource(source);
  }

  // Copy the list of children because RecursivelyAttachBeginFrameSource() can
  // modify |frame_sink_source_map_| and invalidate iterators.
  base::flat_set<FrameSinkId> children = mapping.children;
  for (const FrameSinkId& child : children)
    RecursivelyAttachBeginFrameSource(child, source);
}

void FrameSinkManagerImpl::RecursivelyDetachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return;

  auto& mapping = iter->second;
  if (mapping.source == source) {
    mapping.source = nullptr;
    auto client_iter = compositor_frame_sinks_.find(frame_sink_id);
    if (client_iter != compositor_frame_sinks_.end())
      client_iter->second.support->SetBeginFrameSource(nullptr);
  }

  // Delete the FrameSinkSourceMapping for |frame_sink_id| if empty.
  if (!mapping.has_children()) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // Copy the list of children because RecursivelyDetachBeginFrameSource() can
  // modify |frame_sink_source_map_| and invalidate iterators.
  base::flat_set<FrameSinkId> children = mapping.children;
  for (const FrameSinkId& child : children)
    RecursivelyDetachBeginFrameSource(child, source);
}

CapturableFrameSink* FrameSinkManagerImpl::FindCapturableFrameSink(
    const FrameSinkId& frame_sink_id) {
  const auto it = compositor_frame_sinks_.find(frame_sink_id);
  if (it == compositor_frame_sinks_.end())
    return nullptr;
  return it->second.support;
}

void FrameSinkManagerImpl::OnCapturerConnectionLost(
    FrameSinkVideoCapturerImpl* capturer) {
  video_capturers_.erase(capturer);
}

bool FrameSinkManagerImpl::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  auto iter = frame_sink_source_map_.find(child_frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return false;

  for (const FrameSinkId& child : iter->second.children) {
    if (child == search_frame_sink_id)
      return true;
    if (ChildContains(child, search_frame_sink_id))
      return true;
  }
  return false;
}

void FrameSinkManagerImpl::OnSurfaceCreated(const SurfaceId& surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (client_) {
    client_->OnSurfaceCreated(surface_id);
  } else {
    // There is no client to assign an owner for the temporary reference, so we
    // can drop the temporary reference safely.
    if (surface_manager_.using_surface_references())
      surface_manager_.DropTemporaryReference(surface_id);
  }
}

void FrameSinkManagerImpl::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  if (client_)
    client_->OnFirstSurfaceActivation(surface_info);
}

void FrameSinkManagerImpl::OnSurfaceActivated(const SurfaceId& surface_id) {}

bool FrameSinkManagerImpl::OnSurfaceDamaged(const SurfaceId& surface_id,
                                            const BeginFrameAck& ack) {
  return false;
}

void FrameSinkManagerImpl::OnSurfaceDiscarded(const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnSurfaceDestroyed(const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                                   const BeginFrameArgs& args) {
}

void FrameSinkManagerImpl::OnClientConnectionLost(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_)
    client_->OnClientConnectionClosed(frame_sink_id);
}

void FrameSinkManagerImpl::SubmitHitTestRegionList(
    const SurfaceId& surface_id,
    uint64_t frame_index,
    mojom::HitTestRegionListPtr hit_test_region_list) {
  hit_test_manager_.SubmitHitTestRegionList(surface_id, frame_index,
                                            std::move(hit_test_region_list));
}

uint64_t FrameSinkManagerImpl::GetActiveFrameIndex(
    const SurfaceId& surface_id) {
  return surface_manager_.GetSurfaceForId(surface_id)->GetActiveFrameIndex();
}

void FrameSinkManagerImpl::OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                                               uint32_t frame_token) {
  if (client_)
    client_->OnFrameTokenChanged(frame_sink_id, frame_token);
}

void FrameSinkManagerImpl::OnAggregatedHitTestRegionListUpdated(
    const FrameSinkId& frame_sink_id,
    mojo::ScopedSharedBufferHandle active_handle,
    uint32_t active_handle_size,
    mojo::ScopedSharedBufferHandle idle_handle,
    uint32_t idle_handle_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->OnAggregatedHitTestRegionListUpdated(
        frame_sink_id, std::move(active_handle), active_handle_size,
        std::move(idle_handle), idle_handle_size);
  }
}

void FrameSinkManagerImpl::SwitchActiveAggregatedHitTestRegionList(
    const FrameSinkId& frame_sink_id,
    uint8_t active_handle_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->SwitchActiveAggregatedHitTestRegionList(frame_sink_id,
                                                     active_handle_index);
  }
}

VideoDetector* FrameSinkManagerImpl::CreateVideoDetectorForTesting(
    std::unique_ptr<base::TickClock> tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!video_detector_);
  video_detector_ = std::make_unique<VideoDetector>(
      surface_manager(), std::move(tick_clock), task_runner);
  return video_detector_.get();
}

}  // namespace viz
