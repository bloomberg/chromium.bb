// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_listener.h"

#include <errno.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/common/nacl_messages.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/trusted/service_runtime/sel_main_chrome.h"

#if defined(OS_LINUX)
#include "content/public/common/child_process_sandbox_support_linux.h"
#endif

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

namespace {
#if defined(OS_MACOSX)

// On Mac OS X, shm_open() works in the sandbox but does not give us
// an FD that we can map as PROT_EXEC.  Rather than doing an IPC to
// get an executable SHM region when CreateMemoryObject() is called,
// we preallocate one on startup, since NaCl's sel_ldr only needs one
// of them.  This saves a round trip.

base::subtle::Atomic32 g_shm_fd = -1;

int CreateMemoryObject(size_t size, int executable) {
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

#elif defined(OS_LINUX)

int CreateMemoryObject(size_t size, int executable) {
  return content::MakeSharedMemorySegmentViaIPC(size, executable);
}

#endif
}  // namespace

NaClListener::NaClListener() : debug_enabled_(false) {}

NaClListener::~NaClListener() {}

void NaClListener::Listen() {
  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);
  IPC::Channel channel(channel_name, IPC::Channel::MODE_CLIENT, this);
  CHECK(channel.Connect());
  MessageLoop::current()->Run();
}

bool NaClListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClListener, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStartSelLdr)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClListener::OnStartSelLdr(std::vector<nacl::FileDescriptor> handles,
                                 bool enable_exception_handling) {
  struct NaClChromeMainArgs *args = NaClChromeMainArgsCreate();
  if (args == NULL) {
    LOG(ERROR) << "NaClChromeMainArgsCreate() failed";
    return;
  }

#if defined(OS_LINUX) || defined(OS_MACOSX)
  args->create_memory_object_func = CreateMemoryObject;
# if defined(OS_MACOSX)
  CHECK(handles.size() >= 1);
  g_shm_fd = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();
# endif
#endif

  CHECK(handles.size() >= 1);
  NaClHandle irt_handle = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();

#if defined(OS_WIN)
  args->irt_fd = _open_osfhandle(reinterpret_cast<intptr_t>(irt_handle),
                                 _O_RDONLY | _O_BINARY);
  if (args->irt_fd < 0) {
    LOG(ERROR) << "_open_osfhandle() failed";
    return;
  }
#else
  args->irt_fd = irt_handle;
#endif

  CHECK(handles.size() == 1);
  args->imc_bootstrap_handle = nacl::ToNativeHandle(handles[0]);
  args->enable_exception_handling = enable_exception_handling;
  args->enable_debug_stub = debug_enabled_;
  NaClChromeMainStart(args);
  NOTREACHED();
}
