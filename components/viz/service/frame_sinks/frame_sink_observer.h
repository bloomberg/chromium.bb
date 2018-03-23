// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_OBSERVER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_OBSERVER_H_

namespace viz {

class CompositorFrameSinkSupport;
class FrameSinkId;

class FrameSinkObserver {
 public:
  // Called when FrameSinkId is registered
  virtual void OnRegisteredFrameSinkId(const FrameSinkId& frame_sink_id) = 0;

  // Called when FrameSinkId is being invalidated
  virtual void OnInvalidatedFrameSinkId(const FrameSinkId& frame_sink_id) = 0;

  // Called when RootCompositorFrameSinkImpl is created
  virtual void OnCreatedRootCompositorFrameSink(
      const FrameSinkId& frame_sink_id) = 0;

  // Called when CompositorFrameSinkImpl is created
  virtual void OnCreatedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) = 0;

  // Called when [Root]CompositorFrameSinkImpl is about to be destroyed
  virtual void OnDestroyedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) = 0;

  // Called when |parent_frame_sink_id| becomes a parent of
  // |child_frame_sink_id|
  virtual void OnRegisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) = 0;

  // Called when |parent_frame_sink_id| stops being a parent of
  // |child_frame_sink_id|
  virtual void OnUnregisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) = 0;

  // Called on creating of CompositorFrameSinkSupport
  virtual void OnRegisteredCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id,
      CompositorFrameSinkSupport* support) = 0;

  // Called on destroying CompositorFrameSinkSupport
  virtual void OnUnregisteredCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_OBSERVER_H_
