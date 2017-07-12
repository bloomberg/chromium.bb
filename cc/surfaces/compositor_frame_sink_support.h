// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_COMPOSITOR_FRAME_SINK_SUPPORT_H_
#define CC_SURFACES_COMPOSITOR_FRAME_SINK_SUPPORT_H_

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/compositor_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/frame_sink_manager_client.h"
#include "cc/surfaces/referenced_surface_tracker.h"
#include "cc/surfaces/surface_client.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_resource_holder.h"
#include "cc/surfaces/surface_resource_holder_client.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class CompositorFrameSinkSupportClient;
class FrameSinkManager;
class Surface;
class SurfaceManager;

class CC_SURFACES_EXPORT CompositorFrameSinkSupport
    : public BeginFrameObserver,
      public SurfaceResourceHolderClient,
      public FrameSinkManagerClient,
      public SurfaceClient {
 public:
  static std::unique_ptr<CompositorFrameSinkSupport> Create(
      CompositorFrameSinkSupportClient* client,
      FrameSinkManager* frame_sink_manager,
      const viz::FrameSinkId& frame_sink_id,
      bool is_root,
      bool handles_frame_sink_id_invalidation,
      bool needs_sync_tokens);

  ~CompositorFrameSinkSupport() override;

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  FrameSinkManager* frame_sink_manager() { return frame_sink_manager_; }
  SurfaceManager* surface_manager() { return surface_manager_; }

  // SurfaceClient implementation.
  void OnSurfaceActivated(Surface* surface) override;
  void RefResources(
      const std::vector<TransferableResource>& resources) override;
  void UnrefResources(const std::vector<ReturnedResource>& resources) override;
  void ReturnResources(const std::vector<ReturnedResource>& resources) override;
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override;

  // FrameSinkManagerClient implementation.
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override;

  void EvictCurrentSurface();
  void SetNeedsBeginFrame(bool needs_begin_frame);
  void DidNotProduceFrame(const BeginFrameAck& ack);
  bool SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                             CompositorFrame frame);
  void RequestCopyOfSurface(std::unique_ptr<CopyOutputRequest> request);
  void ClaimTemporaryReference(const viz::SurfaceId& surface_id);

  Surface* GetCurrentSurfaceForTesting();

 protected:
  CompositorFrameSinkSupport(CompositorFrameSinkSupportClient* client,
                             const viz::FrameSinkId& frame_sink_id,
                             bool is_root,
                             bool handles_frame_sink_id_invalidation,
                             bool needs_sync_tokens);

  void Init(FrameSinkManager* frame_sink_manager);

 private:
  // Updates surface references using |active_referenced_surfaces| from the most
  // recent CompositorFrame. This will add and remove top-level root references
  // if |is_root_| is true and |local_surface_id| has changed. Modifies surface
  // references stored in SurfaceManager.
  void UpdateSurfaceReferences(
      const viz::LocalSurfaceId& local_surface_id,
      const std::vector<viz::SurfaceId>& active_referenced_surfaces);

  // Creates a surface reference from the top-level root to |surface_id|.
  SurfaceReference MakeTopLevelRootReference(const viz::SurfaceId& surface_id);

  void DidReceiveCompositorFrameAck();
  void WillDrawSurface(const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect);

  // BeginFrameObserver implementation.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  void UpdateNeedsBeginFramesInternal();
  Surface* CreateSurface(const SurfaceInfo& surface_info);

  CompositorFrameSinkSupportClient* const client_;

  FrameSinkManager* frame_sink_manager_ = nullptr;
  SurfaceManager* surface_manager_ = nullptr;

  const viz::FrameSinkId frame_sink_id_;
  viz::SurfaceId current_surface_id_;

  // If this contains a value then a surface reference from the top-level root
  // to viz::SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()) was
  // added. This will not contain a value if |is_root_| is false.
  base::Optional<viz::LocalSurfaceId> referenced_local_surface_id_;

  SurfaceResourceHolder surface_resource_holder_;

  // Counts the number of CompositorFrames that have been submitted and have not
  // yet received an ACK.
  int ack_pending_count_ = 0;
  std::vector<ReturnedResource> surface_returned_resources_;

  // The begin frame source being observered. Null if none.
  BeginFrameSource* begin_frame_source_ = nullptr;

  // The last begin frame args generated by the begin frame source.
  BeginFrameArgs last_begin_frame_args_;

  // Whether a request for begin frames has been issued.
  bool needs_begin_frame_ = false;

  // Whether or not a frame observer has been added.
  bool added_frame_observer_ = false;

  const bool is_root_;
  const bool needs_sync_tokens_;
  bool seen_first_frame_activation_ = false;

  // TODO(staraz): Remove this flag once ui::Compositor no longer needs to call
  // RegisterFrameSinkId().
  // A surfaceSequence's validity is bound to the lifetime of the parent
  // FrameSink that created it. We track the lifetime of FrameSinks through
  // RegisterFrameSinkId and InvalidateFrameSinkId. During startup and GPU
  // restart, a SurfaceSequence created by the top most layer compositor may be
  // used prior to the creation of the associated CompositorFrameSinkSupport.
  // CompositorFrameSinkSupport is created asynchronously when a new GPU channel
  // is established. Once we switch to SurfaceReferences, this ordering concern
  // goes away and we can remove this bool.
  const bool handles_frame_sink_id_invalidation_;

  base::WeakPtrFactory<CompositorFrameSinkSupport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkSupport);
};

}  // namespace cc

#endif  // CC_SURFACES_COMPOSITOR_FRAME_SINK_SUPPORT_H_
