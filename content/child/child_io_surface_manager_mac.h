// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_IO_SURFACE_MANAGER_MAC_H_
#define CONTENT_CHILD_CHILD_IO_SURFACE_MANAGER_MAC_H_

#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/common/mac/io_surface_manager_token.h"
#include "ui/gfx/mac/io_surface_manager.h"

namespace content {

// Implementation of IOSurfaceManager that registers and acquires IOSurfaces
// through a Mach service.
class CONTENT_EXPORT ChildIOSurfaceManager : public gfx::IOSurfaceManager {
 public:
  // Returns the global ChildIOSurfaceManager.
  static ChildIOSurfaceManager* GetInstance();

  // Overridden from IOSurfaceManager:
  bool RegisterIOSurface(gfx::IOSurfaceId io_surface_id,
                         int client_id,
                         IOSurfaceRef io_surface) override;
  void UnregisterIOSurface(gfx::IOSurfaceId io_surface_id,
                           int client_id) override;
  IOSurfaceRef AcquireIOSurface(gfx::IOSurfaceId io_surface_id) override;

  // Set the service Mach port. Ownership of |service_port| is passed to the
  // manager.
  // Note: This can be called on any thread but must happen before the
  // thread-safe IOSurfaceManager interface is used. It is the responsibility
  // of users of this class to ensure there are no races.
  void set_service_port(mach_port_t service_port) {
    service_port_.reset(service_port);
  }

  // Set the token used when communicating with the Mach service.
  // Note: This can be called on any thread but must happen before the
  // thread-safe IOSurfaceManager interface is used. It is the responsibility
  // of users of this class to ensure there are no races.
  void set_token(const IOSurfaceManagerToken& token) {
    token_ = token;
  }

 private:
  friend struct base::DefaultSingletonTraits<ChildIOSurfaceManager>;

  ChildIOSurfaceManager();
  ~ChildIOSurfaceManager() override;

  base::mac::ScopedMachSendRight service_port_;
  IOSurfaceManagerToken token_;

  DISALLOW_COPY_AND_ASSIGN(ChildIOSurfaceManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_IO_SURFACE_MANAGER_MAC_H_
