// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_FRAMESINK_MANAGER_H_
#define CC_SURFACES_FRAMESINK_MANAGER_H_

#include <stdint.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class BeginFrameSource;
class SurfaceFactoryClient;

namespace test {
class CompositorFrameSinkSupportTest;
}

class CC_SURFACES_EXPORT FrameSinkManager {
 public:
  FrameSinkManager();
  ~FrameSinkManager();

  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id);

  // Invalidate a frame_sink_id that might still have associated sequences,
  // possibly because a renderer process has crashed.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id);

  // SurfaceFactoryClient, hierarchy, and BeginFrameSource can be registered
  // and unregistered in any order with respect to each other.
  //
  // This happens in practice, e.g. the relationship to between ui::Compositor /
  // DelegatedFrameHost is known before ui::Compositor has a surface/client).
  // However, DelegatedFrameHost can register itself as a client before its
  // relationship with the ui::Compositor is known.

  // Associates a SurfaceFactoryClient with the  frame_sink_id it  uses.
  // SurfaceFactoryClient and framesink allocators have a 1:1 mapping.
  // Caller guarantees the client is alive between register/unregister.
  void RegisterSurfaceFactoryClient(const FrameSinkId& frame_sink_id,
                                    SurfaceFactoryClient* client);
  void UnregisterSurfaceFactoryClient(const FrameSinkId& frame_sink_id);

  // Associates a |source| with a particular framesink.  That framesink and
  // any children of that framesink with valid clients can potentially use
  // that |source|.
  void RegisterBeginFrameSource(BeginFrameSource* source,
                                const FrameSinkId& frame_sink_id);
  void UnregisterBeginFrameSource(BeginFrameSource* source);

  // Register a relationship between two framesinks.  This relationship means
  // that surfaces from the child framesik will be displayed in the parent.
  // Children are allowed to use any begin frame source that their parent can
  // use.
  void RegisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                  const FrameSinkId& child_frame_sink_id);
  void UnregisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                    const FrameSinkId& child_frame_sink_id);

  // Export list of valid frame_sink_ids for SatisfyDestructionDeps in surface
  // may be removed later when References replace Sequences
  std::unordered_set<FrameSinkId, FrameSinkIdHash>* GetValidFrameSinkIds(){
    return &valid_frame_sink_ids_;
  }

 private:
  friend class test::CompositorFrameSinkSupportTest;

  void RecursivelyAttachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);
  void RecursivelyDetachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         BeginFrameSource* source);

  // Returns true if |child framesink| is or has |search_frame_sink_id| as a
  // child.
  bool ChildContains(const FrameSinkId& child_frame_sink_id,
                     const FrameSinkId& search_frame_sink_id) const;

  // Set of valid framesink Ids. When a framesink Id  is removed from
  // this set, any remaining (surface) sequences with that framesink are
  // considered satisfied.
  std::unordered_set<FrameSinkId, FrameSinkIdHash> valid_frame_sink_ids_;

  // Begin frame source routing. Both BeginFrameSource and SurfaceFactoryClient
  // pointers guaranteed alive by callers until unregistered.
  struct FrameSinkSourceMapping {
    FrameSinkSourceMapping();
    FrameSinkSourceMapping(const FrameSinkSourceMapping& other);
    ~FrameSinkSourceMapping();
    bool has_children() const { return !children.empty(); }
    // The currently assigned begin frame source for this client.
    BeginFrameSource* source;
    // This represents a dag of parent -> children mapping.
    std::vector<FrameSinkId> children;
  };

  std::unordered_map<FrameSinkId, SurfaceFactoryClient*, FrameSinkIdHash>
      clients_;

  std::unordered_map<FrameSinkId, FrameSinkSourceMapping, FrameSinkIdHash>
      frame_sink_source_map_;

  // Set of which sources are registered to which frmesinks.  Any child
  // that is implicitly using this framesink must be reachable by the
  // parent in the dag.
  std::unordered_map<BeginFrameSource*, FrameSinkId> registered_sources_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManager);
};

}  // namespace cc

#endif  // CC_SURFACES_FRAMESINK_MANAGER_H_
