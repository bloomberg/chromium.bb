// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_listener.h"

#include <errno.h>
#include <stdlib.h>

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/loader/nacl_ipc_adapter.h"
#include "components/nacl/loader/nacl_validation_db.h"
#include "components/nacl/loader/nacl_validation_query.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/nacl_app.h"
#include "native_client/src/public/nacl_file_info.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_LINUX)
#include "components/nacl/loader/nonsfi/irt_random.h"
#include "components/nacl/loader/nonsfi/nonsfi_main.h"
#include "content/public/common/child_process_sandbox_support_linux.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#endif

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>

#include "content/public/common/sandbox_init.h"
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

#elif defined(OS_WIN)

NaClListener* g_listener;

// We wrap the function to convert the bool return value to an int.
int BrokerDuplicateHandle(NaClHandle source_handle,
                          uint32_t process_id,
                          NaClHandle* target_handle,
                          uint32_t desired_access,
                          uint32_t options) {
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
}

int AttachDebugExceptionHandler(const void* info, size_t info_size) {
  std::string info_string(reinterpret_cast<const char*>(info), info_size);
  bool result = false;
  if (!g_listener->Send(new NaClProcessMsg_AttachDebugExceptionHandler(
           info_string, &result)))
    return false;
  return result;
}

void DebugStubPortSelectedHandler(uint16_t port) {
  g_listener->Send(new NaClProcessHostMsg_DebugStubPortSelected(port));
}

#endif

// Creates the PPAPI IPC channel between the NaCl IRT and the host
// (browser/renderer) process, and starts to listen it on the thread where
// the given message_loop_proxy runs.
// Also, creates and sets the corresponding NaClDesc to the given nap with
// the FD #.
void SetUpIPCAdapter(IPC::ChannelHandle* handle,
                     scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
                     struct NaClApp* nap,
                     int nacl_fd) {
  scoped_refptr<NaClIPCAdapter> ipc_adapter(
      new NaClIPCAdapter(*handle, message_loop_proxy.get()));
  ipc_adapter->ConnectChannel();
#if defined(OS_POSIX)
  handle->socket =
      base::FileDescriptor(ipc_adapter->TakeClientFileDescriptor(), true);
#endif

  // Pass a NaClDesc to the untrusted side. This will hold a ref to the
  // NaClIPCAdapter.
  NaClAppSetDesc(nap, nacl_fd, ipc_adapter->MakeNaClDesc());
}

}  // namespace

class BrowserValidationDBProxy : public NaClValidationDB {
 public:
  explicit BrowserValidationDBProxy(NaClListener* listener)
      : listener_(listener) {
  }

  virtual bool QueryKnownToValidate(const std::string& signature) OVERRIDE {
    // Initialize to false so that if the Send fails to write to the return
    // value we're safe.  For example if the message is (for some reason)
    // dispatched as an async message the return parameter will not be written.
    bool result = false;
    if (!listener_->Send(new NaClProcessMsg_QueryKnownToValidate(signature,
                                                                 &result))) {
      LOG(ERROR) << "Failed to query NaCl validation cache.";
      result = false;
    }
    return result;
  }

  virtual void SetKnownToValidate(const std::string& signature) OVERRIDE {
    // Caching is optional: NaCl will still work correctly if the IPC fails.
    if (!listener_->Send(new NaClProcessMsg_SetKnownToValidate(signature))) {
      LOG(ERROR) << "Failed to update NaCl validation cache.";
    }
  }

  virtual bool ResolveFileToken(struct NaClFileToken* file_token,
                                int32* fd, std::string* path) OVERRIDE {
    *fd = -1;
    *path = "";
    if (!NaClFileTokenIsValid(file_token)) {
      return false;
    }
    IPC::PlatformFileForTransit ipc_fd = IPC::InvalidPlatformFileForTransit();
    base::FilePath ipc_path;
    if (!listener_->Send(new NaClProcessMsg_ResolveFileToken(file_token->lo,
                                                             file_token->hi,
                                                             &ipc_fd,
                                                             &ipc_path))) {
      return false;
    }
    if (ipc_fd == IPC::InvalidPlatformFileForTransit()) {
      return false;
    }
    base::PlatformFile handle =
        IPC::PlatformFileForTransitToPlatformFile(ipc_fd);
#if defined(OS_WIN)
    // On Windows, valid handles are 32 bit unsigned integers so this is safe.
    *fd = reinterpret_cast<uintptr_t>(handle);
#else
    *fd = handle;
#endif
    // It doesn't matter if the path is invalid UTF8 as long as it's consistent
    // and unforgeable.
    *path = ipc_path.AsUTF8Unsafe();
    return true;
  }

 private:
  // The listener never dies, otherwise this might be a dangling reference.
  NaClListener* listener_;
};


NaClListener::NaClListener() : shutdown_event_(true, false),
                               io_thread_("NaCl_IOThread"),
                               uses_nonsfi_mode_(false),
#if defined(OS_LINUX)
                               prereserved_sandbox_size_(0),
#endif
#if defined(OS_POSIX)
                               number_of_cores_(-1),  // unknown/error
#endif
                               main_loop_(NULL) {
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
#if defined(OS_WIN)
  DCHECK(g_listener == NULL);
  g_listener = this;
#endif
}

NaClListener::~NaClListener() {
  NOTREACHED();
  shutdown_event_.Signal();
#if defined(OS_WIN)
  g_listener = NULL;
#endif
}

bool NaClListener::Send(IPC::Message* msg) {
  DCHECK(main_loop_ != NULL);
  if (base::MessageLoop::current() == main_loop_) {
    // This thread owns the channel.
    return channel_->Send(msg);
  } else {
    // This thread does not own the channel.
    return filter_->Send(msg);
  }
}

void NaClListener::Listen() {
  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);
  channel_ = IPC::SyncChannel::Create(
      this, io_thread_.message_loop_proxy().get(), &shutdown_event_);
  filter_ = new IPC::SyncMessageFilter(&shutdown_event_);
  channel_->AddFilter(filter_.get());
  channel_->Init(channel_name, IPC::Channel::MODE_CLIENT, true);
  main_loop_ = base::MessageLoop::current();
  main_loop_->Run();
}

bool NaClListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClListener, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStart)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClListener::OnStart(const nacl::NaClStartParams& params) {
  if (uses_nonsfi_mode_) {
    StartNonSfi(params);
    return;
  }

#if defined(OS_LINUX) || defined(OS_MACOSX)
  int urandom_fd = dup(base::GetUrandomFD());
  if (urandom_fd < 0) {
    LOG(ERROR) << "Failed to dup() the urandom FD";
    return;
  }
  NaClChromeMainSetUrandomFd(urandom_fd);
#endif

  struct NaClApp* nap = NULL;
  NaClChromeMainInit();
  nap = NaClAppCreate();
  if (nap == NULL) {
    LOG(ERROR) << "NaClAppCreate() failed";
    return;
  }

  IPC::ChannelHandle browser_handle;
  IPC::ChannelHandle ppapi_renderer_handle;

  if (params.enable_ipc_proxy) {
    browser_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    ppapi_renderer_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");

    // Create the PPAPI IPC channels between the NaCl IRT and the host
    // (browser/renderer) processes. The IRT uses these channels to
    // communicate with the host and to initialize the IPC dispatchers.
    SetUpIPCAdapter(&browser_handle, io_thread_.message_loop_proxy(),
                    nap, NACL_CHROME_DESC_BASE);
    SetUpIPCAdapter(&ppapi_renderer_handle, io_thread_.message_loop_proxy(),
                    nap, NACL_CHROME_DESC_BASE + 1);
  }

  IPC::ChannelHandle trusted_renderer_handle = CreateTrustedListener(
      io_thread_.message_loop_proxy(), &shutdown_event_);
  if (!Send(new NaClProcessHostMsg_PpapiChannelsCreated(
          browser_handle, ppapi_renderer_handle,
          trusted_renderer_handle, IPC::ChannelHandle())))
    LOG(ERROR) << "Failed to send IPC channel handle to NaClProcessHost.";

  std::vector<nacl::FileDescriptor> handles = params.handles;
  struct NaClChromeMainArgs* args = NaClChromeMainArgsCreate();
  if (args == NULL) {
    LOG(ERROR) << "NaClChromeMainArgsCreate() failed";
    return;
  }

#if defined(OS_LINUX) || defined(OS_MACOSX)
  args->number_of_cores = number_of_cores_;
  args->create_memory_object_func = CreateMemoryObject;
# if defined(OS_MACOSX)
  CHECK(handles.size() >= 1);
  g_shm_fd = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();
# endif
#endif

  if (params.uses_irt) {
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
  } else {
    // Otherwise, the IRT handle is not even sent.
    args->irt_fd = -1;
  }

  if (params.validation_cache_enabled) {
    // SHA256 block size.
    CHECK_EQ(params.validation_cache_key.length(), (size_t) 64);
    // The cache structure is not freed and exists until the NaCl process exits.
    args->validation_cache = CreateValidationCache(
        new BrowserValidationDBProxy(this), params.validation_cache_key,
        params.version);
  }

  CHECK(handles.size() == 1);
  args->imc_bootstrap_handle = nacl::ToNativeHandle(handles[0]);
  args->enable_exception_handling = params.enable_exception_handling;
  args->enable_debug_stub = params.enable_debug_stub;
  args->enable_dyncode_syscalls = params.enable_dyncode_syscalls;
  if (!params.enable_dyncode_syscalls) {
    // Bound the initial nexe's code segment size under PNaCl to
    // reduce the chance of a code spraying attack succeeding (see
    // https://code.google.com/p/nativeclient/issues/detail?id=3572).
    // We assume that !params.enable_dyncode_syscalls is synonymous
    // with PNaCl.  We can't apply this arbitrary limit outside of
    // PNaCl because it might break existing NaCl apps, and this limit
    // is only useful if the dyncode syscalls are disabled.
    args->initial_nexe_max_code_bytes = 64 << 20;  // 64 MB

    // Indicate that this is a PNaCl module.
    // TODO(jvoung): Plumb through something indicating that this is PNaCl
    // instead of relying on enable_dyncode_syscalls.
    args->pnacl_mode = 1;
  }
#if defined(OS_LINUX) || defined(OS_MACOSX)
  args->debug_stub_server_bound_socket_fd = nacl::ToNativeHandle(
      params.debug_stub_server_bound_socket);
#endif
#if defined(OS_WIN)
  args->broker_duplicate_handle_func = BrokerDuplicateHandle;
  args->attach_debug_exception_handler_func = AttachDebugExceptionHandler;
  args->debug_stub_server_port_selected_handler_func =
      DebugStubPortSelectedHandler;
#endif
#if defined(OS_LINUX)
  args->prereserved_sandbox_size = prereserved_sandbox_size_;
#endif

  NaClChromeMainStartApp(nap, args);
}

void NaClListener::StartNonSfi(const nacl::NaClStartParams& params) {
#if !defined(OS_LINUX)
  NOTREACHED() << "Non-SFI NaCl is only supported on Linux";
#else
  // Random number source initialization.
  nacl::nonsfi::SetUrandomFd(base::GetUrandomFD());

  IPC::ChannelHandle browser_handle;
  IPC::ChannelHandle ppapi_renderer_handle;
  IPC::ChannelHandle manifest_service_handle;

  if (params.enable_ipc_proxy) {
    browser_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    ppapi_renderer_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    manifest_service_handle =
        IPC::Channel::GenerateVerifiedChannelID("nacl");

    // In non-SFI mode, we neither intercept nor rewrite the message using
    // NaClIPCAdapter, and the channels are connected between the plugin and
    // the hosts directly. So, the IPC::Channel instances will be created in
    // the plugin side, because the IPC::Listener needs to live on the
    // plugin's main thread. However, on initialization (i.e. before loading
    // the plugin binary), the FD needs to be passed to the hosts. So, here
    // we create raw FD pairs, and pass the client side FDs to the hosts,
    // and the server side FDs to the plugin.
    int browser_server_ppapi_fd;
    int browser_client_ppapi_fd;
    int renderer_server_ppapi_fd;
    int renderer_client_ppapi_fd;
    int manifest_service_server_fd;
    int manifest_service_client_fd;
    if (!IPC::SocketPair(
            &browser_server_ppapi_fd, &browser_client_ppapi_fd) ||
        !IPC::SocketPair(
            &renderer_server_ppapi_fd, &renderer_client_ppapi_fd) ||
        !IPC::SocketPair(
            &manifest_service_server_fd, &manifest_service_client_fd)) {
      LOG(ERROR) << "Failed to create sockets for IPC.";
      return;
    }

    // Set the plugin IPC channel FDs.
    ppapi::SetIPCFileDescriptors(browser_server_ppapi_fd,
                                 renderer_server_ppapi_fd,
                                 manifest_service_server_fd);
    ppapi::StartUpPlugin();

    // Send back to the client side IPC channel FD to the host.
    browser_handle.socket =
        base::FileDescriptor(browser_client_ppapi_fd, true);
    ppapi_renderer_handle.socket =
        base::FileDescriptor(renderer_client_ppapi_fd, true);
    manifest_service_handle.socket =
        base::FileDescriptor(manifest_service_client_fd, true);
  }

  // TODO(teravest): Do we plan on using this renderer handle for nexe loading
  // for non-SFI? Right now, passing an empty channel handle instead causes
  // hangs, so we'll keep it.
  IPC::ChannelHandle trusted_renderer_handle = CreateTrustedListener(
      io_thread_.message_loop_proxy(), &shutdown_event_);
  if (!Send(new NaClProcessHostMsg_PpapiChannelsCreated(
          browser_handle, ppapi_renderer_handle,
          trusted_renderer_handle, manifest_service_handle)))
    LOG(ERROR) << "Failed to send IPC channel handle to NaClProcessHost.";

  // Ensure that the validation cache key (used as an extra input to the
  // validation cache's hashing) isn't exposed accidentally.
  CHECK(!params.validation_cache_enabled);
  CHECK(params.validation_cache_key.size() == 0);
  CHECK(params.version.size() == 0);
  // Ensure that a debug stub FD isn't passed through accidentally.
  CHECK(!params.enable_debug_stub);
  CHECK(params.debug_stub_server_bound_socket.fd == -1);

  CHECK(!params.uses_irt);
  CHECK(params.handles.empty());

  CHECK(params.nexe_file != IPC::InvalidPlatformFileForTransit());
  nacl::nonsfi::MainStart(
      NaClDescIoDescFromDescAllocCtor(
          IPC::PlatformFileForTransitToPlatformFile(params.nexe_file),
          NACL_ABI_O_RDONLY));
#endif  // defined(OS_LINUX)
}

IPC::ChannelHandle NaClListener::CreateTrustedListener(
    base::MessageLoopProxy* message_loop_proxy,
    base::WaitableEvent* shutdown_event) {
  // The argument passed to GenerateVerifiedChannelID() here MUST be "nacl".
  // Using an alternate channel name prevents the pipe from being created on
  // Windows when the sandbox is enabled.
  IPC::ChannelHandle trusted_renderer_handle =
      IPC::Channel::GenerateVerifiedChannelID("nacl");
  trusted_listener_ = new NaClTrustedListener(
      trusted_renderer_handle, io_thread_.message_loop_proxy().get());
#if defined(OS_POSIX)
  trusted_renderer_handle.socket = base::FileDescriptor(
      trusted_listener_->TakeClientFileDescriptor(), true);
#endif
  return trusted_renderer_handle;
}
