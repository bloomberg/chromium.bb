// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_registry.h"

#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
base::LazyInstance<SynchronousCompositorRegistry> g_compositor_registry =
    LAZY_INSTANCE_INITIALIZER;
}

// static
SynchronousCompositorRegistry* SynchronousCompositorRegistry::GetInstance() {
  return g_compositor_registry.Pointer();
}

SynchronousCompositorRegistry::SynchronousCompositorRegistry() {
  DCHECK(CalledOnValidThread());
}

SynchronousCompositorRegistry::~SynchronousCompositorRegistry() {
  DCHECK(CalledOnValidThread());
}

void SynchronousCompositorRegistry::RegisterCompositor(
    int routing_id,
    SynchronousCompositorImpl* compositor) {
  DCHECK(CalledOnValidThread());
  DCHECK(compositor);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.compositor);
  entry.compositor = compositor;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistry::UnregisterCompositor(
    int routing_id,
    SynchronousCompositorImpl* compositor) {
  DCHECK(CalledOnValidThread());
  DCHECK(compositor);
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  DCHECK_EQ(compositor, entry.compositor);

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.compositor = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorRegistry::RegisterBeginFrameSource(
    int routing_id,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source) {
  DCHECK(CalledOnValidThread());
  DCHECK(begin_frame_source);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.begin_frame_source);
  entry.begin_frame_source = begin_frame_source;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistry::UnregisterBeginFrameSource(
    int routing_id,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source) {
  DCHECK(CalledOnValidThread());
  DCHECK(begin_frame_source);
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  DCHECK_EQ(begin_frame_source, entry.begin_frame_source);

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.begin_frame_source = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorRegistry::RegisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.output_surface);
  entry.output_surface = output_surface;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistry::UnregisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface);
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  DCHECK_EQ(output_surface, entry.output_surface);

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.output_surface = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorRegistry::CheckIsReady(int routing_id) {
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  if (entry.IsReady()) {
    entry.compositor->DidInitializeRendererObjects(entry.output_surface,
                                                   entry.begin_frame_source);
  }
}

void SynchronousCompositorRegistry::UnregisterObjects(int routing_id) {
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  DCHECK(entry.IsReady());
  entry.compositor->DidDestroyRendererObjects();
}

void SynchronousCompositorRegistry::RemoveEntryIfNeeded(int routing_id) {
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  if (!entry.compositor && !entry.begin_frame_source && !entry.output_surface) {
    entry_map_.erase(routing_id);
  }
}

bool SynchronousCompositorRegistry::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

SynchronousCompositorRegistry::Entry::Entry()
    : compositor(nullptr),
      begin_frame_source(nullptr),
      output_surface(nullptr) {
}

bool SynchronousCompositorRegistry::Entry::IsReady() {
  return compositor && begin_frame_source && output_surface;
}

}  // namespace content
