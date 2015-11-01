// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_IO_SURFACE_MANAGER_MAC_H_
#define CONTENT_BROWSER_BROWSER_IO_SURFACE_MANAGER_MAC_H_

#include <mach/mach.h>

#include <map>
#include <utility>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/mac/io_surface_manager_messages.h"
#include "content/common/mac/io_surface_manager_token.h"
#include "ui/gfx/mac/io_surface_manager.h"

namespace content {

// TODO(ericrk): Use gfx::GenericSharedMemoryId as the |io_surface_id| in
// this file. Allows for more type-safe usage of GpuMemoryBufferIds as the
// type of the |io_surface_id|, as it is a typedef of
// gfx::GenericSharedMemoryId.

// Implementation of IOSurfaceManager that provides a mechanism for child
// processes to register and acquire IOSurfaces through a Mach service.
class CONTENT_EXPORT BrowserIOSurfaceManager : public gfx::IOSurfaceManager {
 public:
  // Returns the global BrowserIOSurfaceManager.
  static BrowserIOSurfaceManager* GetInstance();

  // Look up the IOSurfaceManager service port that's been registered with
  // the bootstrap server. |pid| is the process ID of the service.
  static base::mac::ScopedMachSendRight LookupServicePort(pid_t pid);

  // Returns the name of the service registered with the bootstrap server.
  static std::string GetMachPortName();

  // Overridden from IOSurfaceManager:
  bool RegisterIOSurface(gfx::IOSurfaceId io_surface_id,
                         int client_id,
                         IOSurfaceRef io_surface) override;
  void UnregisterIOSurface(gfx::IOSurfaceId io_surface_id,
                           int client_id) override;
  IOSurfaceRef AcquireIOSurface(gfx::IOSurfaceId io_surface_id) override;

  // Performs any necessary setup that cannot happen in the constructor.
  void EnsureRunning();

  // Generate a unique unguessable token that the GPU process can use to
  // register/unregister IOSurface for use by clients.
  IOSurfaceManagerToken GenerateGpuProcessToken();

  // Invalidate the previously generated GPU process token.
  void InvalidateGpuProcessToken();

  // Generate a unique unguessable token that the child process associated
  // |child_process_id| can use to acquire IOSurface references.
  IOSurfaceManagerToken GenerateChildProcessToken(int child_process_id);

  // Invalidate a previously generated child process token.
  void InvalidateChildProcessToken(const IOSurfaceManagerToken& token);

 private:
  friend class BrowserIOSurfaceManagerTest;
  friend struct base::DefaultSingletonTraits<BrowserIOSurfaceManager>;

  BrowserIOSurfaceManager();
  ~BrowserIOSurfaceManager() override;

  // Performs any initialization work.
  bool Initialize();

  // Message handler that is invoked on |dispatch_source_| when an
  // incoming message needs to be received.
  void HandleRequest();

  // Message handlers that are invoked from HandleRequest.
  void HandleRegisterIOSurfaceRequest(
      const IOSurfaceManagerHostMsg_RegisterIOSurface& request,
      IOSurfaceManagerMsg_RegisterIOSurfaceReply* reply);
  bool HandleUnregisterIOSurfaceRequest(
      const IOSurfaceManagerHostMsg_UnregisterIOSurface& request);
  void HandleAcquireIOSurfaceRequest(
      const IOSurfaceManagerHostMsg_AcquireIOSurface& request,
      IOSurfaceManagerMsg_AcquireIOSurfaceReply* reply);

  // Whether or not the class has been initialized.
  bool initialized_;

  // The Mach port on which the server listens.
  base::mac::ScopedMachReceiveRight server_port_;

  // The dispatch source and queue on which Mach messages will be received.
  scoped_ptr<base::DispatchSourceMach> dispatch_source_;

  // Stores the IOSurfaces for all GPU clients. The key contains the IOSurface
  // id and the Child process unique id of the owner.
  using IOSurfaceMapKey = std::pair<gfx::IOSurfaceId, int>;
  using IOSurfaceMap =
      base::ScopedPtrHashMap<IOSurfaceMapKey,
                             scoped_ptr<base::mac::ScopedMachSendRight>>;
  IOSurfaceMap io_surfaces_;

  // Stores the Child process unique id (RenderProcessHost ID) for every
  // token.
  using ChildProcessIdMap = std::map<IOSurfaceManagerToken, int>;
  ChildProcessIdMap child_process_ids_;

  // Stores the GPU process token.
  IOSurfaceManagerToken gpu_process_token_;

  // Mutex that guards |initialized_|, |io_surfaces_|, |child_process_ids_|
  // and |gpu_process_token_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(BrowserIOSurfaceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_IO_SURFACE_MANAGER_MAC_H_
