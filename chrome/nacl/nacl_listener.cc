// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "native_client/src/shared/imc/nacl_imc.h"

#if defined(OS_LINUX)
#include "content/public/common/child_process_sandbox_support_linux.h"
#endif

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

// This is ugly.  We need an interface header file for the exported
// sel_ldr interfaces.
// TODO(gregoryd,sehr): Add an interface header.
#if defined(OS_WIN)
typedef HANDLE NaClHandle;
#else
typedef int NaClHandle;
#endif  // NaClHandle

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

}  // namespace
#endif  // defined(OS_MACOSX)

extern "C" void NaClMainForChromium(int handle_count,
                                    const NaClHandle* handles,
                                    int debug);
extern "C" void NaClSetIrtFileDesc(int fd);

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

void NaClListener::OnStartSelLdr(std::vector<nacl::FileDescriptor> handles) {
#if defined(OS_LINUX)
  nacl::SetCreateMemoryObjectFunc(content::MakeSharedMemorySegmentViaIPC);
#elif defined(OS_MACOSX)
  nacl::SetCreateMemoryObjectFunc(CreateMemoryObject);
  CHECK(handles.size() >= 1);
  g_shm_fd = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();
#endif

  CHECK(handles.size() >= 1);
  NaClHandle irt_handle = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();

#if defined(OS_WIN)
  int irt_desc = _open_osfhandle(reinterpret_cast<intptr_t>(irt_handle),
                                 _O_RDONLY | _O_BINARY);
  if (irt_desc < 0) {
    LOG(ERROR) << "_open_osfhandle() failed";
    return;
  }
#else
  int irt_desc = irt_handle;
#endif

  NaClSetIrtFileDesc(irt_desc);

  scoped_array<NaClHandle> array(new NaClHandle[handles.size()]);
  for (size_t i = 0; i < handles.size(); i++) {
    array[i] = nacl::ToNativeHandle(handles[i]);
  }
  NaClMainForChromium(static_cast<int>(handles.size()), array.get(),
                      debug_enabled_);
  NOTREACHED();
}
