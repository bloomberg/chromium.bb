// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_io_surface_manager_mac.h"

#include <servers/bootstrap.h>

#include <string>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"

namespace content {
namespace {

// Returns the Mach port name to use when sending or receiving messages. |pid|
// is the process ID of the service.
std::string GetMachPortNameByPid(pid_t pid) {
  return base::StringPrintf("%s.iosurfacemgr.%d", base::mac::BaseBundleID(),
                            pid);
}

// Amount of time to wait before giving up when sending a reply message.
const int kSendReplyTimeoutMs = 100;

}  // namespace

// static
BrowserIOSurfaceManager* BrowserIOSurfaceManager::GetInstance() {
  return base::Singleton<
      BrowserIOSurfaceManager,
      base::LeakySingletonTraits<BrowserIOSurfaceManager>>::get();
}

// static
base::mac::ScopedMachSendRight BrowserIOSurfaceManager::LookupServicePort(
    pid_t pid) {
  // Look up the named IOSurfaceManager port that's been registered with
  // the bootstrap server.
  mach_port_t port;
  kern_return_t kr =
      bootstrap_look_up(bootstrap_port,
                        GetMachPortNameByPid(pid).c_str(),
                        &port);
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_look_up";
    return base::mac::ScopedMachSendRight();
  }

  return base::mac::ScopedMachSendRight(port);
}

// static
std::string BrowserIOSurfaceManager::GetMachPortName() {
  return GetMachPortNameByPid(getpid());
}

bool BrowserIOSurfaceManager::RegisterIOSurface(IOSurfaceId io_surface_id,
                                                int client_id,
                                                IOSurfaceRef io_surface) {
  base::AutoLock lock(lock_);

  IOSurfaceMapKey key(io_surface_id, client_id);
  DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
  io_surfaces_.add(key, make_scoped_ptr(new base::mac::ScopedMachSendRight(
                            IOSurfaceCreateMachPort(io_surface))));
  return true;
}

void BrowserIOSurfaceManager::UnregisterIOSurface(IOSurfaceId io_surface_id,
                                                  int client_id) {
  base::AutoLock lock(lock_);

  IOSurfaceMapKey key(io_surface_id, client_id);
  DCHECK(io_surfaces_.find(key) != io_surfaces_.end());
  io_surfaces_.erase(key);
}

IOSurfaceRef BrowserIOSurfaceManager::AcquireIOSurface(
    IOSurfaceId io_surface_id) {
  base::AutoLock lock(lock_);

  IOSurfaceMapKey key(
      io_surface_id,
      BrowserGpuChannelHostFactory::instance()->GetGpuChannelId());
  auto it = io_surfaces_.find(key);
  if (it == io_surfaces_.end()) {
    LOG(ERROR) << "Invalid Id for IOSurface " << io_surface_id.id;
    return nullptr;
  }

  return IOSurfaceLookupFromMachPort(it->second->get());
}

void BrowserIOSurfaceManager::EnsureRunning() {
  base::AutoLock lock(lock_);

  if (initialized_)
    return;

  // Do not attempt to reinitialize in the event of failure.
  initialized_ = true;

  if (!Initialize()) {
    LOG(ERROR) << "Failed to initialize the BrowserIOSurfaceManager";
  }
}

IOSurfaceManagerToken BrowserIOSurfaceManager::GenerateGpuProcessToken() {
  base::AutoLock lock(lock_);

  DCHECK(gpu_process_token_.IsZero());
  gpu_process_token_ = IOSurfaceManagerToken::Generate();
  DCHECK(gpu_process_token_.Verify());
  return gpu_process_token_;
}

void BrowserIOSurfaceManager::InvalidateGpuProcessToken() {
  base::AutoLock lock(lock_);

  DCHECK(!gpu_process_token_.IsZero());
  gpu_process_token_.SetZero();
  io_surfaces_.clear();
}

IOSurfaceManagerToken BrowserIOSurfaceManager::GenerateChildProcessToken(
    int child_process_id) {
  base::AutoLock lock(lock_);

  IOSurfaceManagerToken token = IOSurfaceManagerToken::Generate();
  DCHECK(token.Verify());
  child_process_ids_[token] = child_process_id;
  return token;
}

void BrowserIOSurfaceManager::InvalidateChildProcessToken(
    const IOSurfaceManagerToken& token) {
  base::AutoLock lock(lock_);

  DCHECK(child_process_ids_.find(token) != child_process_ids_.end());
  child_process_ids_.erase(token);
}

BrowserIOSurfaceManager::BrowserIOSurfaceManager() : initialized_(false) {
}

BrowserIOSurfaceManager::~BrowserIOSurfaceManager() {
}

bool BrowserIOSurfaceManager::Initialize() {
  lock_.AssertAcquired();
  DCHECK(!server_port_.is_valid());

  // Check in with launchd and publish the service name.
  mach_port_t port;
  kern_return_t kr = bootstrap_check_in(
      bootstrap_port, GetMachPortName().c_str(), &port);
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_check_in";
    return false;
  }
  server_port_.reset(port);

  // Start the dispatch source.
  std::string queue_name =
      base::StringPrintf("%s.IOSurfaceManager", base::mac::BaseBundleID());
  dispatch_source_.reset(
      new base::DispatchSourceMach(queue_name.c_str(), server_port_.get(), ^{
        HandleRequest();
      }));
  dispatch_source_->Resume();

  return true;
}

void BrowserIOSurfaceManager::HandleRequest() {
  struct {
    union {
      mach_msg_header_t header;
      IOSurfaceManagerHostMsg_RegisterIOSurface register_io_surface;
      IOSurfaceManagerHostMsg_UnregisterIOSurface unregister_io_surface;
      IOSurfaceManagerHostMsg_AcquireIOSurface acquire_io_surface;
    } msg;
    mach_msg_trailer_t trailer;
  } request = {{{0}}};
  request.msg.header.msgh_size = sizeof(request);
  request.msg.header.msgh_local_port = server_port_.get();

  kern_return_t kr =
      mach_msg(&request.msg.header, MACH_RCV_MSG, 0, sizeof(request),
               server_port_.get(), MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return;
  }

  union {
    mach_msg_header_t header;
    IOSurfaceManagerMsg_RegisterIOSurfaceReply register_io_surface;
    IOSurfaceManagerMsg_AcquireIOSurfaceReply acquire_io_surface;
  } reply = {{0}};

  switch (request.msg.header.msgh_id) {
    case IOSurfaceManagerHostMsg_RegisterIOSurface::ID:
      HandleRegisterIOSurfaceRequest(request.msg.register_io_surface,
                                     &reply.register_io_surface);
      break;
    case IOSurfaceManagerHostMsg_UnregisterIOSurface::ID:
      HandleUnregisterIOSurfaceRequest(request.msg.unregister_io_surface);
      // Unregister requests are asynchronous and do not require a reply as
      // there is no guarantee for how quickly an IO surface is removed from
      // the IOSurfaceManager instance after it has been deleted by a child
      // process.
      return;
    case IOSurfaceManagerHostMsg_AcquireIOSurface::ID:
      HandleAcquireIOSurfaceRequest(request.msg.acquire_io_surface,
                                    &reply.acquire_io_surface);
      break;
    default:
      LOG(ERROR) << "Unknown message received!";
      return;
  }

  kr = mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                reply.header.msgh_size, 0, MACH_PORT_NULL, kSendReplyTimeoutMs,
                MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
  }
}

void BrowserIOSurfaceManager::HandleRegisterIOSurfaceRequest(
    const IOSurfaceManagerHostMsg_RegisterIOSurface& request,
    IOSurfaceManagerMsg_RegisterIOSurfaceReply* reply) {
  base::AutoLock lock(lock_);

  reply->header.msgh_bits = MACH_MSGH_BITS_REMOTE(request.header.msgh_bits);
  reply->header.msgh_remote_port = request.header.msgh_remote_port;
  reply->header.msgh_size = sizeof(*reply);
  reply->body.msgh_descriptor_count = 0;
  reply->result = false;

  IOSurfaceManagerToken token;
  static_assert(sizeof(request.token_name) == sizeof(token.name),
                "Mach message token size doesn't match expectation.");
  token.SetName(request.token_name);
  if (token.IsZero() || token != gpu_process_token_) {
    LOG(ERROR) << "Illegal message from non-GPU process!";
    return;
  }

  IOSurfaceMapKey key(IOSurfaceId(request.io_surface_id), request.client_id);
  io_surfaces_.add(key, make_scoped_ptr(new base::mac::ScopedMachSendRight(
                            request.io_surface_port.name)));
  reply->result = true;
}

bool BrowserIOSurfaceManager::HandleUnregisterIOSurfaceRequest(
    const IOSurfaceManagerHostMsg_UnregisterIOSurface& request) {
  base::AutoLock lock(lock_);

  IOSurfaceManagerToken token;
  static_assert(sizeof(request.token_name) == sizeof(token.name),
                "Mach message token size doesn't match expectation.");
  token.SetName(request.token_name);
  if (token.IsZero() || token != gpu_process_token_) {
    LOG(ERROR) << "Illegal message from non-GPU process!";
    return false;
  }

  IOSurfaceMapKey key(IOSurfaceId(request.io_surface_id), request.client_id);
  io_surfaces_.erase(key);
  return true;
}

void BrowserIOSurfaceManager::HandleAcquireIOSurfaceRequest(
    const IOSurfaceManagerHostMsg_AcquireIOSurface& request,
    IOSurfaceManagerMsg_AcquireIOSurfaceReply* reply) {
  base::AutoLock lock(lock_);

  reply->header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(request.header.msgh_bits) | MACH_MSGH_BITS_COMPLEX;
  reply->header.msgh_remote_port = request.header.msgh_remote_port;
  reply->header.msgh_size = sizeof(*reply);
  reply->body.msgh_descriptor_count = 0;
  reply->result = false;
  reply->io_surface_port.name = MACH_PORT_NULL;
  reply->io_surface_port.disposition = 0;
  reply->io_surface_port.type = 0;

  IOSurfaceManagerToken token;
  static_assert(sizeof(request.token_name) == sizeof(token.name),
                "Mach message token size doesn't match expectation.");
  token.SetName(request.token_name);
  auto child_process_id_it = child_process_ids_.find(token);
  if (child_process_id_it == child_process_ids_.end()) {
    LOG(ERROR) << "Illegal message from non-child process!";
    return;
  }

  reply->result = true;
  IOSurfaceMapKey key(IOSurfaceId(request.io_surface_id),
                      child_process_id_it->second);
  auto it = io_surfaces_.find(key);
  if (it == io_surfaces_.end()) {
    LOG(ERROR) << "Invalid Id for IOSurface " << request.io_surface_id;
    return;
  }

  reply->body.msgh_descriptor_count = 1;
  reply->io_surface_port.name = it->second->get();
  reply->io_surface_port.disposition = MACH_MSG_TYPE_COPY_SEND;
  reply->io_surface_port.type = MACH_MSG_PORT_DESCRIPTOR;
}

}  // namespace content
