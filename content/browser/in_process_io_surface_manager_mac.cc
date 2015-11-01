// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_io_surface_manager_mac.h"

#include "base/logging.h"

namespace content {

// static
InProcessIOSurfaceManager* InProcessIOSurfaceManager::GetInstance() {
  return base::Singleton<
      InProcessIOSurfaceManager,
      base::LeakySingletonTraits<InProcessIOSurfaceManager>>::get();
}

bool InProcessIOSurfaceManager::RegisterIOSurface(
    gfx::IOSurfaceId io_surface_id,
    int client_id,
    IOSurfaceRef io_surface) {
  base::AutoLock lock(lock_);

  DCHECK(io_surfaces_.find(io_surface_id) == io_surfaces_.end());
  io_surfaces_.add(io_surface_id,
                   make_scoped_ptr(new base::mac::ScopedMachSendRight(
                       IOSurfaceCreateMachPort(io_surface))));
  return true;
}

void InProcessIOSurfaceManager::UnregisterIOSurface(
    gfx::IOSurfaceId io_surface_id,
    int client_id) {
  base::AutoLock lock(lock_);

  DCHECK(io_surfaces_.find(io_surface_id) != io_surfaces_.end());
  io_surfaces_.erase(io_surface_id);
}

IOSurfaceRef InProcessIOSurfaceManager::AcquireIOSurface(
    gfx::IOSurfaceId io_surface_id) {
  base::AutoLock lock(lock_);

  DCHECK(io_surfaces_.find(io_surface_id) != io_surfaces_.end());
  return IOSurfaceLookupFromMachPort(io_surfaces_.get(io_surface_id)->get());
}

InProcessIOSurfaceManager::InProcessIOSurfaceManager() {
}

InProcessIOSurfaceManager::~InProcessIOSurfaceManager() {
}

}  // namespace content
