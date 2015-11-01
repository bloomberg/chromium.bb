// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_IO_SURFACE_MANAGER_MAC_H_
#define CONTENT_BROWSER_IN_PROCESS_IO_SURFACE_MANAGER_MAC_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ui/gfx/mac/io_surface_manager.h"

namespace content {

class CONTENT_EXPORT InProcessIOSurfaceManager : public gfx::IOSurfaceManager {
 public:
  static InProcessIOSurfaceManager* GetInstance();

  // Overridden from IOSurfaceManager:
  bool RegisterIOSurface(gfx::IOSurfaceId io_surface_id,
                         int client_id,
                         IOSurfaceRef io_surface) override;
  void UnregisterIOSurface(gfx::IOSurfaceId io_surface_id,
                           int client_id) override;
  IOSurfaceRef AcquireIOSurface(gfx::IOSurfaceId io_surface_id) override;

 private:
  friend struct base::DefaultSingletonTraits<InProcessIOSurfaceManager>;

  InProcessIOSurfaceManager();
  ~InProcessIOSurfaceManager() override;

  using IOSurfaceMap =
      base::ScopedPtrHashMap<gfx::IOSurfaceId,
                             scoped_ptr<base::mac::ScopedMachSendRight>>;
  IOSurfaceMap io_surfaces_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(InProcessIOSurfaceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_PROCESS_IO_SURFACE_MANAGER_MAC_H_
