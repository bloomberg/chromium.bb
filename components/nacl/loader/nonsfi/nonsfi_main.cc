// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_main.h"

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/nacl/loader/nonsfi/elf_loader.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/public/secure_service.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/nacl_irt/plugin_startup.h"

namespace nacl {
namespace nonsfi {
namespace {

typedef void (*EntryPointType)(uintptr_t*);

class PluginMainDelegate : public base::PlatformThread::Delegate {
 public:
  explicit PluginMainDelegate(EntryPointType entry_point)
      : entry_point_(entry_point) {
  }

  virtual ~PluginMainDelegate() {
  }

  virtual void ThreadMain() OVERRIDE {
    base::PlatformThread::SetName("NaClMainThread");

    // This will only happen once per process, so we give the permission to
    // create Singletons.
    base::ThreadRestrictions::SetSingletonAllowed(true);
    ppapi::StartUpPlugin();
    uintptr_t info[] = {
      0,  // Do not use fini.
      0,  // envc.
      0,  // argc.
      0,  // Null terminate for argv.
      0,  // Null terminate for envv.
      AT_SYSINFO,
      reinterpret_cast<uintptr_t>(&NaClIrtInterface),
      AT_NULL,
      0,  // Null terminate for auxv.
    };
    entry_point_(info);
  }

 private:
  EntryPointType entry_point_;
};

// Default stack size of the plugin main thread. We heuristically chose 16M.
const size_t kStackSize = (16 << 20);

struct NaClDescUnrefer {
  void operator()(struct NaClDesc* desc) const {
    NaClDescUnref(desc);
  }
};

void LoadModuleRpc(struct NaClSrpcRpc* rpc,
                   struct NaClSrpcArg** in_args,
                   struct NaClSrpcArg** out_args,
                   struct NaClSrpcClosure* done_cls) {
  rpc->result = NACL_SRPC_RESULT_INTERNAL;

  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> desc(in_args[0]->u.hval);
  ElfImage image;
  if (image.Read(desc.get()) != LOAD_OK) {
    LOG(ERROR) << "LoadModuleRpc: Failed to read binary.";
    return;
  }

  if (image.Load(desc.get()) != LOAD_OK) {
    LOG(ERROR) << "LoadModuleRpc: Failed to load the image";
    return;
  }

  EntryPointType entry_point =
      reinterpret_cast<EntryPointType>(image.entry_point());
  if (!base::PlatformThread::CreateNonJoinable(
          kStackSize, new PluginMainDelegate(entry_point))) {
    LOG(ERROR) << "LoadModuleRpc: Failed to create plugin main thread.";
    return;
  }

  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

const static struct NaClSrpcHandlerDesc kNonSfiServiceHandlers[] = {
  { NACL_SECURE_SERVICE_LOAD_MODULE, LoadModuleRpc, },
  { static_cast<const char*>(NULL), static_cast<NaClSrpcMethod>(NULL), },
};

// Creates two socketpairs to communicate with the host process.
void CreateSecureSocketPair(struct NaClDesc* secure_pair[2],
                            struct NaClDesc* pair[2]) {
  // Set up a secure pair.
  if (NaClCommonDescMakeBoundSock(secure_pair)) {
    LOG(FATAL) << "Cound not create secure service socket\n";
  }

  // Set up a service pair.
  if (NaClCommonDescMakeBoundSock(pair)) {
    LOG(FATAL) << "Could not create service socket";
  }
}

// Wraps handle by NaClDesc, and sends secure_service_address and
// service_address via the created descriptor.
struct NaClDesc* SetUpBootstrapChannel(NaClHandle handle,
                                       struct NaClDesc* secure_service_address,
                                       struct NaClDesc* service_address) {
  if (secure_service_address == NULL) {
    LOG(FATAL) << "SetUpBootstrapChannel: secure_service_address is not set";
  }

  if (service_address == NULL) {
    LOG(FATAL) << "SetUpBootstrapChannel: secure_service_address is not set";
  }

  struct NaClDescImcDesc* channel =
      static_cast<struct NaClDescImcDesc*>(malloc(sizeof *channel));
  if (channel == NULL) {
    LOG(FATAL) << "SetUpBootstrapChannel: no memory";
  }

  if (!NaClDescImcDescCtor(channel, handle)) {
    LOG(FATAL) << "SetUpBootstrapChannel: cannot construct IMC descriptor "
               << "object for inherited descriptor: " << handle;
  }

  // Send the descriptors to the host.
  struct NaClDesc* descs[2] = {
    secure_service_address,
    service_address,
  };

  struct NaClImcTypedMsgHdr hdr;
  hdr.iov = static_cast<struct NaClImcMsgIoVec*>(NULL);
  hdr.iov_length = 0;
  hdr.ndescv = descs;
  hdr.ndesc_length = NACL_ARRAY_SIZE(descs);
  hdr.flags = 0;

  ssize_t error = (*NACL_VTBL(NaClDesc, channel)->SendMsg)(
      reinterpret_cast<struct NaClDesc*>(channel), &hdr, 0);
  if (error) {
    LOG(FATAL) << "SetUpBootstrapChannel: SendMsg failed, error = " << error;
  }
  return reinterpret_cast<struct NaClDesc*>(channel);
}

// Starts to listen to the port and runs the server loop.
void ServiceAccept(struct NaClDesc* port) {
  struct NaClDesc* connected_desc = NULL;
  int status = (*NACL_VTBL(NaClDesc, port)->AcceptConn)(port, &connected_desc);
  if (status) {
    LOG(ERROR) << "ServiceAccept: Failed to accept " << status;
    return;
  }

  NaClSrpcServerLoop(connected_desc, kNonSfiServiceHandlers, NULL);
}

}  // namespace

void MainStart(NaClHandle imc_bootstrap_handle) {
  NaClSrpcModuleInit();

  struct NaClDesc* secure_pair[2] = { NULL, NULL };
  struct NaClDesc* pair[2] = { NULL, NULL };
  CreateSecureSocketPair(secure_pair, pair);
  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> secure_port(secure_pair[0]);
  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> secure_address(
       secure_pair[1]);
  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> service_port(pair[0]);
  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> service_address(pair[1]);

  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> channel(
       SetUpBootstrapChannel(imc_bootstrap_handle,
                             secure_address.get(), service_address.get()));
  if (!channel) {
    LOG(ERROR) << "MainStart: Failed to set up bootstrap channel.";
    return;
  }

  // Start the SRPC server loop.
  ServiceAccept(secure_port.get());
}

}  // namespace nonsfi
}  // namespace nacl
