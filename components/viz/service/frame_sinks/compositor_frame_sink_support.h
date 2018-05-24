// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/referenced_surface_tracker.h"
#include "components/viz/service/frame_sinks/surface_resource_holder.h"
#include "components/viz/service/frame_sinks/surface_resource_holder_client.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"

namespace viz {

class FrameSinkManagerImpl;
class LatestLocalSurfaceIdLookupDelegate;
class Surface;
class SurfaceManager;

class VIZ_SERVICE_EXPORT CompositorFrameSinkSupport
    : public BeginFrameObserver,
      public SurfaceResourceHolderClient,
      public SurfaceClient,
      public CapturableFrameSink {
 public:
  // Possible outcomes of MaybeSubmitCompositorFrame().
  enum SubmitResult {
    ACCEPTED,
    COPY_OUTPUT_REQUESTS_NOT_ALLOWED,
    SURFACE_INVARIANTS_VIOLATION,
  };

  using AggregatedDamageCallback =
      base::RepeatingCallback<void(const LocalSurfaceId& local_surface_id,
                                   const gfx::Size& frame_size_in_pixels,
                                   const gfx::Rect& damage_rect,
                                   base::TimeTicks expected_display_time)>;

  static const uint64_t kFrameIndexStart = 2;

  CompositorFrameSinkSupport(mojom::CompositorFrameSinkClient* client,
                             FrameSinkManagerImpl* frame_sink_manager,
                             const FrameSinkId& frame_sink_id,
                             bool is_root,
                             bool needs_sync_tokens);
  ~CompositorFrameSinkSupport() override;

  const FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const SurfaceId& last_activated_surface_id() const {
    return last_activated_surface_id_;
  }

  const LocalSurfaceId& last_activated_local_surface_id() const {
    return last_activated_surface_id_.local_surface_id();
  }

  bool is_root() const { return is_root_; }

  FrameSinkManagerImpl* frame_sink_manager() { return frame_sink_manager_; }

  // Viz hit-test setup is only called when |is_root_| is true (except on
  // android webview).
  void SetUpHitTest(
      LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate);

  // The provided callback will be run every time a surface owned by this object
  // or one of its descendents is determined to be damaged at aggregation time.
  void SetAggregatedDamageCallbackForTesting(AggregatedDamageCallback callback);

  // Sets callback called on destruction.
  void SetDestructionCallback(base::OnceClosure callback);

  // This allows the FrameSinkManagerImpl to pass a BeginFrameSource to use.
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source);

  // SurfaceClient implementation.
  void OnSurfaceActivated(Surface* surface) override;
  void OnSurfaceDiscarded(Surface* surface) override;
  void RefResources(
      const std::vector<TransferableResource>& resources) override;
  void UnrefResources(const std::vector<ReturnedResource>& resources) override;
  void ReturnResources(const std::vector<ReturnedResource>& resources) override;
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override;
  // Takes the CopyOutputRequests that were requested for a surface with at
  // most |local_surface_id|.
  std::vector<std::unique_ptr<CopyOutputRequest>> TakeCopyOutputRequests(
      const LocalSurfaceId& local_surface_id) override;

  // mojom::CompositorFrameSink helpers.
  void SetNeedsBeginFrame(bool needs_begin_frame);
  void SetWantsAnimateOnlyBeginFrames();
  void DidNotProduceFrame(const BeginFrameAck& ack);
  void SubmitCompositorFrame(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      base::Optional<HitTestRegionList> hit_test_region_list = base::nullopt,
      uint64_t submit_time = 0);
  // Returns false if the notification was not valid (a duplicate).
  bool DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const SharedBitmapId& id);
  void DidDeleteSharedBitmap(const SharedBitmapId& id);

  void EvictLastActivatedSurface();

  // Attempts to submit a new CompositorFrame to |local_surface_id| and returns
  // whether the frame was accepted or the reason why it was rejected. If
  // |local_surface_id| hasn't been submitted before then a new Surface will be
  // created for it.
  //
  // This is called by SubmitCompositorFrame(), which DCHECK-fails on a
  // non-accepted result. Prefer calling SubmitCompositorFrame() instead of this
  // method unless the result value affects what the caller will do next.
  SubmitResult MaybeSubmitCompositorFrame(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      base::Optional<HitTestRegionList> hit_test_region_list,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback);

  // CapturableFrameSink implementation.
  void AttachCaptureClient(CapturableFrameSink::Client* client) override;
  void DetachCaptureClient(CapturableFrameSink::Client* client) override;
  gfx::Size GetActiveFrameSize() override;
  void RequestCopyOfOutput(const LocalSurfaceId& local_surface_id,
                           std::unique_ptr<CopyOutputRequest> request) override;
  const CompositorFrameMetadata* GetLastActivatedFrameMetadata() override;

  HitTestAggregator* GetHitTestAggregator();

  // Permits submitted CompositorFrames to contain CopyOutputRequests, for
  // special-case testing purposes only.
  void set_allow_copy_output_requests_for_testing() {
    allow_copy_output_requests_ = true;
  }

  Surface* GetLastCreatedSurfaceForTesting();

  // Maps the |result| from MaybeSubmitCompositorFrame() to a human-readable
  // string.
  static const char* GetSubmitResultAsString(SubmitResult result);

 private:
  friend class FrameSinkManagerTest;

  // Updates surface references using |active_referenced_surfaces| from the most
  // recent CompositorFrame. This will add and remove top-level root references
  // if |is_root_| is true and |local_surface_id| has changed. Modifies surface
  // references stored in SurfaceManager.
  void UpdateSurfaceReferences(
      const LocalSurfaceId& local_surface_id,
      const std::vector<SurfaceId>& active_referenced_surfaces);

  // Creates a surface reference from the top-level root to |surface_id|.
  SurfaceReference MakeTopLevelRootReference(const SurfaceId& surface_id);

  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 base::TimeTicks time,
                                 base::TimeDelta refresh,
                                 uint32_t flags);

  // BeginFrameObserver implementation.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

  void UpdateNeedsBeginFramesInternal();
  Surface* CreateSurface(const SurfaceInfo& surface_info);

  void OnAggregatedDamage(const LocalSurfaceId& local_surface_id,
                          const CompositorFrame& frame,
                          const gfx::Rect& damage_rect,
                          base::TimeTicks expected_display_time) const;

  // For the sync API calls, if we are blocking a client callback, runs it once
  // BeginFrame and FrameAck are done.
  void HandleCallback();

  mojom::CompositorFrameSinkClient* const client_;

  FrameSinkManagerImpl* const frame_sink_manager_;
  SurfaceManager* const surface_manager_;

  const FrameSinkId frame_sink_id_;
  SurfaceId last_activated_surface_id_;
  SurfaceId last_created_surface_id_;

  // If this contains a value then a surface reference from the top-level root
  // to SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()) was
  // added. This will not contain a value if |is_root_| is false.
  base::Optional<LocalSurfaceId> referenced_local_surface_id_;

  SurfaceResourceHolder surface_resource_holder_;

  // This has a HitTestAggregator if and only if |is_root_| is true.
  std::unique_ptr<HitTestAggregator> hit_test_aggregator_;

  // Counts the number of CompositorFrames that have been submitted and have not
  // yet received an ACK.
  int ack_pending_count_ = 0;
  std::vector<ReturnedResource> surface_returned_resources_;

  // The begin frame source being observered. Null if none.
  BeginFrameSource* begin_frame_source_ = nullptr;

  // The last begin frame args generated by the begin frame source.
  BeginFrameArgs last_begin_frame_args_;

  // Whether a request for begin frames has been issued.
  bool client_needs_begin_frame_ = false;

  // Whether or not a frame observer has been added.
  bool added_frame_observer_ = false;

  bool wants_animate_only_begin_frames_ = false;

  const bool is_root_;
  const bool needs_sync_tokens_;

  // By default, this is equivalent to |is_root_|, but may be overridden for
  // testing. Generally, for non-roots, there must not be any CopyOutputRequests
  // contained within submitted CompositorFrames. Otherwise, unprivileged
  // clients would be able to capture content for which they are not authorized.
  bool allow_copy_output_requests_;

  // A callback that will be run at the start of the destructor if set.
  base::OnceClosure destruction_callback_;

  // TODO(crbug.com/754872): Remove once tab capture has moved into VIZ.
  AggregatedDamageCallback aggregated_damage_callback_;

  uint64_t last_frame_index_ = kFrameIndexStart;

  // The video capture clients hooking into this instance to observe frame
  // begins and damage, and then make CopyOutputRequests on the appropriate
  // frames.
  std::vector<CapturableFrameSink::Client*> capture_clients_;

  // The set of SharedBitmapIds that have been reported as allocated to this
  // interface. On closing this interface, the display compositor should drop
  // ownership of the bitmaps with these ids to avoid leaking them.
  std::set<SharedBitmapId> owned_bitmaps_;

  // These are the CopyOutputRequests made on the frame sink (as opposed to
  // being included as a part of a CompositorFrame). They stay here until a
  // Surface with a LocalSurfaceId which is at least the stored LocalSurfaceId
  // takes them. For example, if we store a pair of LocalSurfaceId stored_id and
  // a CopyOutputRequest, then a surface with LocalSurfaceId >= stored_id will
  // take it, but a surface with LocalSurfaceId < stored_id will not. Note that
  // if stored_id is default initialized, then the next surface will take it
  // regardless of its LocalSurfaceId.
  std::vector<std::pair<LocalSurfaceId, std::unique_ptr<CopyOutputRequest>>>
      copy_output_requests_;

  mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback
      compositor_frame_callback_;
  bool callback_received_begin_frame_ = true;
  bool callback_received_receive_ack_ = true;

  base::WeakPtrFactory<CompositorFrameSinkSupport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkSupport);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_
