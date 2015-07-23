// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_IO_SURFACE_MANAGER_MAC_H_
#define CONTENT_CHILD_CHILD_IO_SURFACE_MANAGER_MAC_H_

#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "content/common/mac/io_surface_manager.h"
#include "content/common/mac/io_surface_manager_token.h"

namespace content {

// Implementation of IOSurfaceManager that registers and acquires IOSurfaces
// through a Mach service.
class CONTENT_EXPORT ChildIOSurfaceManager : public IOSurfaceManager {
 public:
  // Returns the global ChildIOSurfaceManager.
  static ChildIOSurfaceManager* GetInstance();

  // Overridden from IOSurfaceManager:
  bool RegisterIOSurface(int io_surface_id,
                         int client_id,
                         IOSurfaceRef io_surface) override;
  void UnregisterIOSurface(int io_surface_id, int client_id) override;
  IOSurfaceRef AcquireIOSurface(int io_surface_id) override;

  // Set the service Mach port. Ownership of |service_port| is passed to the
  // manager.
  void set_service_port(mach_port_t service_port) {
    service_port_.reset(service_port);
  }

  // Set the token used when communicating with the Mach service.
  void set_token(const IOSurfaceManagerToken& token) {
    token_ = token;
#if !defined(NDEBUG)
    set_token_thread_id_ = base::PlatformThread::CurrentRef();
#endif
    set_token_event_.Signal();
  }

 private:
  friend struct DefaultSingletonTraits<ChildIOSurfaceManager>;

  ChildIOSurfaceManager();
  ~ChildIOSurfaceManager() override;

  base::mac::ScopedMachSendRight service_port_;
  IOSurfaceManagerToken token_;
#if !defined(NDEBUG)
  base::PlatformThreadRef set_token_thread_id_;
#endif
  base::WaitableEvent set_token_event_;

  DISALLOW_COPY_AND_ASSIGN(ChildIOSurfaceManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_IO_SURFACE_MANAGER_MAC_H_
