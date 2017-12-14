// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/primary_begin_frame_source.h"
#include "components/viz/service/frame_sinks/video_detector.h"
#include "components/viz/service/hit_test/hit_test_manager.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/video_detector_observer.mojom.h"

namespace viz {

class CapturableFrameSink;
class CompositorFrameSinkSupport;
class DisplayProvider;

// FrameSinkManagerImpl manages BeginFrame hierarchy. This is the implementation
// detail for FrameSinkManagerImpl.
class VIZ_SERVICE_EXPORT FrameSinkManagerImpl : public SurfaceObserver,
                                                public mojom::FrameSinkManager {
 public:
  FrameSinkManagerImpl(SurfaceManager::LifetimeType lifetime_type =
                           SurfaceManager::LifetimeType::REFERENCES,
                       DisplayProvider* display_provider = nullptr);
  ~FrameSinkManagerImpl() override;

  // Binds |this| as a FrameSinkManagerImpl for |request| on |task_runner|. On
  // Mac |task_runner| will be the resize helper task runner. May only be called
  // once.
  void BindAndSetClient(mojom::FrameSinkManagerRequest request,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        mojom::FrameSinkManagerClientPtr client);

  // Sets up a direction connection to |client| without using Mojo.
  void SetLocalClient(mojom::FrameSinkManagerClient* client);

  // mojom::FrameSinkManager implementation:
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id) override;
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) override;
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label) override;
  void CreateRootCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      bool force_software_compositing,
      const RendererSettings& renderer_settings,
      mojom::CompositorFrameSinkAssociatedRequest request,
      mojom::CompositorFrameSinkClientPtr client,
      mojom::DisplayPrivateAssociatedRequest display_private_request,
      mojom::DisplayClientPtr display_client) override;
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      mojom::CompositorFrameSinkRequest request,
      mojom::CompositorFrameSinkClientPtr client) override;
  void RegisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void AssignTemporaryReference(const SurfaceId& surface_id,
                                const FrameSinkId& owner) override;
  void DropTemporaryReference(const SurfaceId& surface_id) override;
  void AddVideoDetectorObserver(
      mojom::VideoDetectorObserverPtr observer) override;

  // CompositorFrameSinkSupport, hierarchy, and BeginFrameSource can be
  // registered and unregistered in any order with respect to each other.
  //
  // This happens in practice, e.g. the relationship to between ui::Compositor /
  // DelegatedFrameHost is known before ui::Compositor has a surface/client).
  // However, DelegatedFrameHost can register itself as a client before its
  // relationship with the ui::Compositor is known.

  // Registers a CompositorFrameSinkSupport for |frame_sink_id|. |frame_sink_id|
  // must be unregistered when |support| is destroyed.
  void RegisterCompositorFrameSinkSupport(const FrameSinkId& frame_sink_id,
                                          CompositorFrameSinkSupport* support);
  void UnregisterCompositorFrameSinkSupport(const FrameSinkId& frame_sink_id);

  // Associates a |source| with a particular framesink.  That framesink and
  // any children of that framesink with valid clients can potentially use
  // that |source|.
  void RegisterBeginFrameSource(BeginFrameSource* source,
                                const FrameSinkId& frame_sink_id);
  void UnregisterBeginFrameSource(BeginFrameSource* source);

  // Returns a stable BeginFrameSource that forwards BeginFrames from the first
  // available BeginFrameSource.
  BeginFrameSource* GetPrimaryBeginFrameSource();

  SurfaceManager* surface_manager() { return &surface_manager_; }

  const HitTestManager* hit_test_manager() { return &hit_test_manager_; }

  // SurfaceObserver implementation.
  void OnSurfaceCreated(const SurfaceId& surface_id) override;
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnSurfaceActivated(const SurfaceId& surface_id) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override;
  void OnSurfaceSubtreeDamaged(const SurfaceId& surface_id) override;

  void OnClientConnectionLost(const FrameSinkId& frame_sink_id);

  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_size);
  void SwitchActiveAggregatedHitTestRegionList(const FrameSinkId& frame_sink_id,
                                               uint8_t active_handle_index);

  // It is necessary to pass |frame_sink_id| by value because the id
  // is owned by the GpuCompositorFrameSink in the map. When the sink is
  // removed from the map, |frame_sink_id| would also be destroyed if it were a
  // reference. But the map can continue to iterate and try to use it. Passing
  // by value avoids this.
  void DestroyCompositorFrameSink(FrameSinkId frame_sink_id);

  void SubmitHitTestRegionList(
      const SurfaceId& surface_id,
      uint64_t frame_index,
      mojom::HitTestRegionListPtr hit_test_region_list);

  // This method is virtual so the implementation can be modified in unit tests.
  virtual uint64_t GetActiveFrameIndex(const SurfaceId& surface_id);

  // Instantiates |video_detector_| for tests where we simulate the passage of
  // time.
  VideoDetector* CreateVideoDetectorForTesting(
      std::unique_ptr<base::TickClock> tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Called when |frame_token| is changed on a submitted CompositorFrame.
  void OnFrameTokenChanged(const FrameSinkId& frame_sink_id,
                           uint32_t frame_token);

 private:
  friend class FrameSinkManagerTest;

  // BeginFrameSource routing information for a FrameSinkId.
  struct FrameSinkSourceMapping {
    FrameSinkSourceMapping();
    FrameSinkSourceMapping(FrameSinkSourceMapping&& other);
    ~FrameSinkSourceMapping();
    FrameSinkSourceMapping& operator=(FrameSinkSourceMapping&& other);

    bool has_children() const { return !children.empty(); }
    // The currently assigned begin frame source for this client.
    BeginFrameSource* source = nullptr;
    // This represents a dag of parent -> children mapping.
    base::flat_set<FrameSinkId> children;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameSinkSourceMapping);
  };

  struct SinkAndSupport {
    SinkAndSupport();
    SinkAndSupport(SinkAndSupport&& other);
    ~SinkAndSupport();
    SinkAndSupport& operator=(SinkAndSupport&& other);

    // CompositorFrameSinks owned here. This will be null if a
    // CompositorFrameSinkSupport is owned externally.
    std::unique_ptr<mojom::CompositorFrameSink> sink;

    // This can be owned by |sink| or owned externally.
    CompositorFrameSinkSupport* support = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(SinkAndSupport);
  };

  void RecursivelyAttachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);
  void RecursivelyDetachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);

  // TODO(crbug.com/754872): To be used by FrameSinkVideoCapturerImpl in
  // an upcoming change.
  CapturableFrameSink* FindCapturableFrameSink(
      const FrameSinkId& frame_sink_id);

  // Returns true if |child framesink| is or has |search_frame_sink_id| as a
  // child.
  bool ChildContains(const FrameSinkId& child_frame_sink_id,
                     const FrameSinkId& search_frame_sink_id) const;

  // Provides a Display for CreateRootCompositorFrameSink().
  DisplayProvider* const display_provider_;

  // Set of BeginFrameSource along with associated FrameSinkIds. Any child
  // that is implicitly using this frame sink must be reachable by the
  // parent in the dag.
  base::flat_map<BeginFrameSource*, FrameSinkId> registered_sources_;

  // Contains FrameSinkId hierarchy and BeginFrameSource mapping.
  base::flat_map<FrameSinkId, FrameSinkSourceMapping> frame_sink_source_map_;

  // Contains (and maybe owns) the CompositorFrameSinkSupport.
  base::flat_map<FrameSinkId, SinkAndSupport> compositor_frame_sinks_;

  PrimaryBeginFrameSource primary_source_;

  // |surface_manager_| should be placed under |primary_source_| so that all
  // surfaces are destroyed before |primary_source_|.
  SurfaceManager surface_manager_;

  HitTestManager hit_test_manager_;

  THREAD_CHECKER(thread_checker_);

  // |video_detector_| is instantiated lazily in order to avoid overhead on
  // platforms that don't need video detection.
  std::unique_ptr<VideoDetector> video_detector_;

  // This will point to |client_ptr_| if using Mojo or a provided client if
  // directly connected. Use this to make function calls.
  mojom::FrameSinkManagerClient* client_ = nullptr;

  mojom::FrameSinkManagerClientPtr client_ptr_;
  mojo::Binding<mojom::FrameSinkManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
