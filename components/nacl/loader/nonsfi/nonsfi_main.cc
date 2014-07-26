// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_main.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/nacl/loader/nonsfi/elf_loader.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

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

}  // namespace

void MainStart(int nexe_file) {
  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> desc(
       NaClDescIoDescFromDescAllocCtor(nexe_file, NACL_ABI_O_RDONLY));
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
}

}  // namespace nonsfi
}  // namespace nacl
