// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_registry_in_proc.h"

#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
base::LazyInstance<SynchronousCompositorRegistryInProc> g_compositor_registry =
    LAZY_INSTANCE_INITIALIZER;
}

// static
SynchronousCompositorRegistryInProc*
SynchronousCompositorRegistryInProc::GetInstance() {
  return g_compositor_registry.Pointer();
}

SynchronousCompositorRegistryInProc::SynchronousCompositorRegistryInProc() {
  DCHECK(CalledOnValidThread());
}

SynchronousCompositorRegistryInProc::~SynchronousCompositorRegistryInProc() {
  DCHECK(CalledOnValidThread());
}

void SynchronousCompositorRegistryInProc::RegisterCompositor(
    int routing_id,
    SynchronousCompositorImpl* compositor) {
  DCHECK(CalledOnValidThread());
  DCHECK(compositor);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.compositor);
  entry.compositor = compositor;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistryInProc::UnregisterCompositor(
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

void SynchronousCompositorRegistryInProc::RegisterBeginFrameSource(
    int routing_id,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source) {
  DCHECK(CalledOnValidThread());
  DCHECK(begin_frame_source);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.begin_frame_source);
  entry.begin_frame_source = begin_frame_source;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistryInProc::UnregisterBeginFrameSource(
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

void SynchronousCompositorRegistryInProc::RegisterOutputSurface(
    int routing_id,
    SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.output_surface);
  entry.output_surface = output_surface;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistryInProc::UnregisterOutputSurface(
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

void SynchronousCompositorRegistryInProc::RegisterInputHandler(
    int routing_id,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(CalledOnValidThread());
  DCHECK(synchronous_input_handler_proxy);
  Entry& entry = entry_map_[routing_id];
  DCHECK(!entry.synchronous_input_handler_proxy);
  entry.synchronous_input_handler_proxy = synchronous_input_handler_proxy;
  CheckIsReady(routing_id);
}

void SynchronousCompositorRegistryInProc::UnregisterInputHandler(
    int routing_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];

  if (entry.IsReady())
    UnregisterObjects(routing_id);
  entry.synchronous_input_handler_proxy = nullptr;
  RemoveEntryIfNeeded(routing_id);
}

void SynchronousCompositorRegistryInProc::CheckIsReady(int routing_id) {
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  if (entry.IsReady()) {
    entry.compositor->DidInitializeRendererObjects(
        entry.output_surface, entry.begin_frame_source,
        entry.synchronous_input_handler_proxy);
  }
}

void SynchronousCompositorRegistryInProc::UnregisterObjects(int routing_id) {
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  DCHECK(entry.IsReady());
  entry.compositor->DidDestroyRendererObjects();
}

void SynchronousCompositorRegistryInProc::RemoveEntryIfNeeded(int routing_id) {
  DCHECK(entry_map_.find(routing_id) != entry_map_.end());
  Entry& entry = entry_map_[routing_id];
  if (!entry.compositor && !entry.begin_frame_source && !entry.output_surface &&
      !entry.synchronous_input_handler_proxy) {
    entry_map_.erase(routing_id);
  }
}

bool SynchronousCompositorRegistryInProc::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

SynchronousCompositorRegistryInProc::Entry::Entry()
    : compositor(nullptr),
      begin_frame_source(nullptr),
      output_surface(nullptr),
      synchronous_input_handler_proxy(nullptr) {}

bool SynchronousCompositorRegistryInProc::Entry::IsReady() {
  return compositor && begin_frame_source && output_surface &&
         synchronous_input_handler_proxy;
}

}  // namespace content
