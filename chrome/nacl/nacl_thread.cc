// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_thread.h"

#include "base/atomicops.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/nacl_messages.h"
#include "native_client/src/shared/imc/nacl_imc.h"

#if defined(OS_LINUX)
#include "chrome/renderer/renderer_sandbox_support_linux.h"
#endif

#if defined(OS_MACOSX)
namespace {

// On Mac OS X, shm_open() works in the sandbox but does not give us
// an FD that we can map as PROT_EXEC.  Rather than doing an IPC to
// get an executable SHM region when CreateMemoryObject() is called,
// we preallocate one on startup, since NaCl's sel_ldr only needs one
// of them.  This saves a round trip.

base::subtle::Atomic32 g_shm_fd = -1;

int CreateMemoryObject(size_t size, bool executable) {
  if (executable && size > 0) {
    int result_fd = base::subtle::NoBarrier_AtomicExchange(&g_shm_fd, -1);
    if (result_fd != -1) {
      // ftruncate() is disallowed by the Mac OS X sandbox and
      // returns EPERM.  Luckily, we can get the same effect with
      // lseek() + write().
      if (lseek(result_fd, size - 1, SEEK_SET) == -1) {
        LOG(ERROR) << "lseek() failed: " << errno;
        return -1;
      }
      if (write(result_fd, "", 1) != 1) {
        LOG(ERROR) << "write() failed: " << errno;
        return -1;
      }
      return result_fd;
    }
  }
  // Fall back to NaCl's default implementation.
  return -1;
}

}
#endif

// This is ugly.  We need an interface header file for the exported
// sel_ldr interfaces.
// TODO(gregoryd,sehr): Add an interface header.
#if defined(OS_WIN)
typedef HANDLE NaClHandle;
#else
typedef int NaClHandle;
#endif  // NaClHandle

// This is currently necessary because we have a conflict between
// NaCl's "struct NaClThread" and Chromium's "class NaClThread".
extern "C" int NaClMainForChromium(int handle_count, const NaClHandle* handles,
                                   int debug);

NaClThread::NaClThread(bool debug) {
  debug_enabled_ = debug ? 1 : 0;
}

NaClThread::~NaClThread() {
}

NaClThread* NaClThread::current() {
  return static_cast<NaClThread*>(ChildThread::current());
}

void NaClThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(NaClThread, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStartSelLdr)
  IPC_END_MESSAGE_MAP()
}

void NaClThread::OnStartSelLdr(std::vector<nacl::FileDescriptor> handles) {
#if defined(OS_LINUX)
  nacl::SetCreateMemoryObjectFunc(
      renderer_sandbox_support::MakeSharedMemorySegmentViaIPC);
#elif defined(OS_MACOSX)
  nacl::SetCreateMemoryObjectFunc(CreateMemoryObject);
  CHECK(handles.size() >= 1);
  g_shm_fd = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();
#endif
  scoped_array<NaClHandle> array(new NaClHandle[handles.size()]);
  for (size_t i = 0; i < handles.size(); i++) {
    array[i] = nacl::ToNativeHandle(handles[i]);
  }
  NaClMainForChromium(static_cast<int>(handles.size()), array.get(),
                      debug_enabled_);
}
