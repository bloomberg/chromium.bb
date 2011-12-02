// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/nacl_host/nacl_process_host.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "content/common/child_process_host.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/shared/imc/nacl_imc.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#elif defined(OS_WIN)
#include "chrome/browser/nacl_host/nacl_broker_service_win.h"
#endif

using content::BrowserThread;

namespace {

void SetCloseOnExec(nacl::Handle fd) {
#if defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFD);
  CHECK_NE(flags, -1);
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  CHECK_EQ(rc, 0);
#endif
}

// Represents shared state for all NaClProcessHost objects in the browser.
// Currently this just handles holding onto the file descriptor for the IRT.
class NaClBrowser {
 public:
  static NaClBrowser* GetInstance() {
    return Singleton<NaClBrowser>::get();
  }

  bool IrtAvailable() const {
    return irt_platform_file_ != base::kInvalidPlatformFileValue;
  }

  base::PlatformFile IrtFile() const {
    CHECK_NE(irt_platform_file_, base::kInvalidPlatformFileValue);
    return irt_platform_file_;
  }

  // Asynchronously attempt to get the IRT open.
  bool EnsureIrtAvailable();

  // Make sure the IRT gets opened and follow up with the reply when it's ready.
  bool MakeIrtAvailable(const base::Closure& reply);

 private:
  base::PlatformFile irt_platform_file_;

  friend struct DefaultSingletonTraits<NaClBrowser>;

  NaClBrowser()
      : irt_platform_file_(base::kInvalidPlatformFileValue)
  {}

  ~NaClBrowser() {
    if (irt_platform_file_ != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(irt_platform_file_);
  }

  void OpenIrtLibraryFile();

  static void DoOpenIrtLibraryFile() {
    GetInstance()->OpenIrtLibraryFile();
  }

  DISALLOW_COPY_AND_ASSIGN(NaClBrowser);
};

}  // namespace

struct NaClProcessHost::NaClInternal {
  std::vector<nacl::Handle> sockets_for_renderer;
  std::vector<nacl::Handle> sockets_for_sel_ldr;
};

static bool RunningOnWOW64() {
#if defined(OS_WIN)
  return (base::win::OSInfo::GetInstance()->wow64_status() ==
          base::win::OSInfo::WOW64_ENABLED);
#else
  return false;
#endif
}

NaClProcessHost::NaClProcessHost(const std::wstring& url)
    : BrowserChildProcessHost(content::PROCESS_TYPE_NACL_LOADER),
      reply_msg_(NULL),
      internal_(new NaClInternal()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  set_name(WideToUTF16Hack(url));
}

NaClProcessHost::~NaClProcessHost() {
  int exit_code;
  GetChildTerminationStatus(&exit_code);
  std::string message =
      base::StringPrintf("NaCl process exited with status %i (0x%x)",
                         exit_code, exit_code);
  if (exit_code == 0) {
    LOG(INFO) << message;
  } else {
    LOG(ERROR) << message;
  }

#if defined(OS_WIN)
  NaClBrokerService::GetInstance()->OnLoaderDied();
#endif

  for (size_t i = 0; i < internal_->sockets_for_renderer.size(); i++) {
    if (nacl::Close(internal_->sockets_for_renderer[i]) != 0) {
      LOG(ERROR) << "nacl::Close() failed";
    }
  }
  for (size_t i = 0; i < internal_->sockets_for_sel_ldr.size(); i++) {
    if (nacl::Close(internal_->sockets_for_sel_ldr[i]) != 0) {
      LOG(ERROR) << "nacl::Close() failed";
    }
  }

  if (reply_msg_) {
    // The process failed to launch for some reason.
    // Don't keep the renderer hanging.
    reply_msg_->set_reply_error();
    chrome_render_message_filter_->Send(reply_msg_);
  }
}

// Attempt to ensure the IRT will be available when we need it, but don't wait.
bool NaClBrowser::EnsureIrtAvailable() {
  if (IrtAvailable())
    return true;

  return BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&NaClBrowser::DoOpenIrtLibraryFile));
}

// We really need the IRT to be available now, so make sure that it is.
// When it's ready, we'll run the reply closure.
bool NaClBrowser::MakeIrtAvailable(const base::Closure& reply) {
  return BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&NaClBrowser::DoOpenIrtLibraryFile), reply);
}

// This is called at browser startup.
// static
void NaClProcessHost::EarlyStartup() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Open the IRT file early to make sure that it isn't replaced out from
  // under us by autoupdate.
  NaClBrowser::GetInstance()->EnsureIrtAvailable();
#endif
}

bool NaClProcessHost::Launch(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    int socket_count,
    IPC::Message* reply_msg) {
  // Place an arbitrary limit on the number of sockets to limit
  // exposure in case the renderer is compromised.  We can increase
  // this if necessary.
  if (socket_count > 8) {
    return false;
  }

  // Start getting the IRT open asynchronously while we launch the NaCl process.
  // We'll make sure this actually finished in OnProcessLaunched, below.
  if (!NaClBrowser::GetInstance()->EnsureIrtAvailable()) {
    LOG(ERROR) << "Cannot launch NaCl process after IRT file open failed";
    return false;
  }

  // Rather than creating a socket pair in the renderer, and passing
  // one side through the browser to sel_ldr, socket pairs are created
  // in the browser and then passed to the renderer and sel_ldr.
  //
  // This is mainly for the benefit of Windows, where sockets cannot
  // be passed in messages, but are copied via DuplicateHandle().
  // This means the sandboxed renderer cannot send handles to the
  // browser process.

  for (int i = 0; i < socket_count; i++) {
    nacl::Handle pair[2];
    // Create a connected socket
    if (nacl::SocketPair(pair) == -1)
      return false;
    internal_->sockets_for_renderer.push_back(pair[0]);
    internal_->sockets_for_sel_ldr.push_back(pair[1]);
    SetCloseOnExec(pair[0]);
    SetCloseOnExec(pair[1]);
  }

  // Launch the process
  if (!LaunchSelLdr()) {
    return false;
  }
  chrome_render_message_filter_ = chrome_render_message_filter;
  reply_msg_ = reply_msg;

  return true;
}

bool NaClProcessHost::LaunchSelLdr() {
  if (!child_process_host()->CreateChannel())
    return false;

  CommandLine::StringType nacl_loader_prefix;
#if defined(OS_POSIX)
  nacl_loader_prefix = CommandLine::ForCurrentProcess()->GetSwitchValueNative(
      switches::kNaClLoaderCmdPrefix);
#endif  // defined(OS_POSIX)

  // Build command line for nacl.

#if defined(OS_MACOSX)
  // The Native Client process needs to be able to allocate a 1GB contiguous
  // region to use as the client environment's virtual address space. ASLR
  // (PIE) interferes with this by making it possible that no gap large enough
  // to accomodate this request will exist in the child process' address
  // space. Disable PIE for NaCl processes. See http://crbug.com/90221 and
  // http://code.google.com/p/nativeclient/issues/detail?id=2043.
  int flags = ChildProcessHost::CHILD_NO_PIE;
#elif defined(OS_LINUX)
  int flags = nacl_loader_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                           ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  nacl::CopyNaClCommandLineArguments(cmd_line);

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kNaClLoaderProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID,
                              child_process_host()->channel_id());
  if (logging::DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  if (!nacl_loader_prefix.empty())
    cmd_line->PrependWrapper(nacl_loader_prefix);

  // On Windows we might need to start the broker process to launch a new loader
#if defined(OS_WIN)
  if (RunningOnWOW64()) {
    return NaClBrokerService::GetInstance()->LaunchLoader(
        this, ASCIIToWide(child_process_host()->channel_id()));
  } else {
    BrowserChildProcessHost::Launch(FilePath(), cmd_line);
  }
#elif defined(OS_POSIX)
  BrowserChildProcessHost::Launch(nacl_loader_prefix.empty(),  // use_zygote
                                  base::environment_vector(),
                                  cmd_line);
#endif

  return true;
}

void NaClProcessHost::OnProcessLaunchedByBroker(base::ProcessHandle handle) {
  set_handle(handle);
  OnProcessLaunched();
}

base::TerminationStatus NaClProcessHost::GetChildTerminationStatus(
    int* exit_code) {
  if (RunningOnWOW64())
    return base::GetTerminationStatus(handle(), exit_code);
  return BrowserChildProcessHost::GetChildTerminationStatus(exit_code);
}

// This only ever runs on the BrowserThread::FILE thread.
// If multiple tasks are posted, the later ones are no-ops.
void NaClBrowser::OpenIrtLibraryFile() {
  if (irt_platform_file_ != base::kInvalidPlatformFileValue)
    // We've already run.
    return;

  FilePath irt_filepath;

  // Allow the IRT library to be overridden via an environment
  // variable.  This allows the NaCl/Chromium integration bot to
  // specify a newly-built IRT rather than using a prebuilt one
  // downloaded via Chromium's DEPS file.  We use the same environment
  // variable that the standalone NaCl PPAPI plugin accepts.
  const char* irt_path_var = getenv("NACL_IRT_LIBRARY");
  if (irt_path_var != NULL) {
    FilePath::StringType path_string(
        irt_path_var, const_cast<const char*>(strchr(irt_path_var, '\0')));
    irt_filepath = FilePath(path_string);
  } else {
    FilePath plugin_dir;
    if (!PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &plugin_dir)) {
      LOG(ERROR) << "Failed to locate the plugins directory";
      return;
    }

    bool on_x86_64 = RunningOnWOW64();
#if defined(__x86_64__)
    on_x86_64 = true;
#endif
    FilePath::StringType irt_name;
    if (on_x86_64) {
      irt_name = FILE_PATH_LITERAL("nacl_irt_x86_64.nexe");
    } else {
      irt_name = FILE_PATH_LITERAL("nacl_irt_x86_32.nexe");
    }

    irt_filepath = plugin_dir.Append(irt_name);
  }

  base::PlatformFileError error_code;
  irt_platform_file_ = base::CreatePlatformFile(irt_filepath,
                                                base::PLATFORM_FILE_OPEN |
                                                base::PLATFORM_FILE_READ,
                                                NULL,
                                                &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Failed to open NaCl IRT file \""
               << irt_filepath.LossyDisplayName()
               << "\": " << error_code;
  }
}

void NaClProcessHost::OnProcessLaunched() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  if (nacl_browser->IrtAvailable()) {
    // The IRT is already open.  Away we go.
    SendStart(nacl_browser->IrtFile());
  } else {
    // We're waiting for the IRT to be open.
    nacl_browser->MakeIrtAvailable(base::Bind(&NaClProcessHost::IrtReady,
                                              weak_factory_.GetWeakPtr()));
  }
}

// The asynchronous attempt to get the IRT file open has completed.
void NaClProcessHost::IrtReady() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  if (nacl_browser->IrtAvailable()) {
    SendStart(nacl_browser->IrtFile());
  } else {
    LOG(ERROR) << "Cannot launch NaCl process after IRT file open failed";
    delete this;
  }
}

static bool SendHandleToSelLdr(
    base::ProcessHandle processh,
    nacl::Handle sourceh, bool close_source,
    std::vector<nacl::FileDescriptor> *handles_for_sel_ldr) {
#if defined(OS_WIN)
  HANDLE channel;
  int flags = DUPLICATE_SAME_ACCESS;
  if (close_source)
    flags |= DUPLICATE_CLOSE_SOURCE;
  if (!DuplicateHandle(GetCurrentProcess(),
                       reinterpret_cast<HANDLE>(sourceh),
                       processh,
                       &channel,
                       0,  // Unused given DUPLICATE_SAME_ACCESS.
                       FALSE,
                       flags)) {
    LOG(ERROR) << "DuplicateHandle() failed";
    return false;
  }
  handles_for_sel_ldr->push_back(
      reinterpret_cast<nacl::FileDescriptor>(channel));
#else
  nacl::FileDescriptor channel;
  channel.fd = sourceh;
  channel.auto_close = close_source;
  handles_for_sel_ldr->push_back(channel);
#endif
  return true;
}

void NaClProcessHost::SendStart(base::PlatformFile irt_file) {
  CHECK_NE(irt_file, base::kInvalidPlatformFileValue);

  std::vector<nacl::FileDescriptor> handles_for_renderer;
  base::ProcessHandle nacl_process_handle;

  for (size_t i = 0; i < internal_->sockets_for_renderer.size(); i++) {
#if defined(OS_WIN)
    // Copy the handle into the renderer process.
    HANDLE handle_in_renderer;
    if (!DuplicateHandle(base::GetCurrentProcessHandle(),
                         reinterpret_cast<HANDLE>(
                             internal_->sockets_for_renderer[i]),
                         chrome_render_message_filter_->peer_handle(),
                         &handle_in_renderer,
                         0,  // Unused given DUPLICATE_SAME_ACCESS.
                         FALSE,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      LOG(ERROR) << "DuplicateHandle() failed";
      delete this;
      return;
    }
    handles_for_renderer.push_back(
        reinterpret_cast<nacl::FileDescriptor>(handle_in_renderer));
#else
    // No need to dup the imc_handle - we don't pass it anywhere else so
    // it cannot be closed.
    nacl::FileDescriptor imc_handle;
    imc_handle.fd = internal_->sockets_for_renderer[i];
    imc_handle.auto_close = true;
    handles_for_renderer.push_back(imc_handle);
#endif
  }

#if defined(OS_WIN)
  // Copy the process handle into the renderer process.
  if (!DuplicateHandle(base::GetCurrentProcessHandle(),
                       handle(),
                       chrome_render_message_filter_->peer_handle(),
                       &nacl_process_handle,
                       PROCESS_DUP_HANDLE,
                       FALSE,
                       0)) {
    LOG(ERROR) << "DuplicateHandle() failed";
    delete this;
    return;
  }
#else
  // We use pid as process handle on Posix
  nacl_process_handle = handle();
#endif

  // Get the pid of the NaCl process
  base::ProcessId nacl_process_id = base::GetProcId(handle());

  ChromeViewHostMsg_LaunchNaCl::WriteReplyParams(
      reply_msg_, handles_for_renderer, nacl_process_handle, nacl_process_id);
  chrome_render_message_filter_->Send(reply_msg_);
  chrome_render_message_filter_ = NULL;
  reply_msg_ = NULL;
  internal_->sockets_for_renderer.clear();

  std::vector<nacl::FileDescriptor> handles_for_sel_ldr;
  for (size_t i = 0; i < internal_->sockets_for_sel_ldr.size(); i++) {
    if (!SendHandleToSelLdr(handle(),
                            internal_->sockets_for_sel_ldr[i], true,
                            &handles_for_sel_ldr)) {
      delete this;
      return;
    }
  }

  // Send over the IRT file handle.  We don't close our own copy!
  if (!SendHandleToSelLdr(handle(), irt_file, false, &handles_for_sel_ldr)) {
    delete this;
    return;
  }

#if defined(OS_MACOSX)
  // For dynamic loading support, NaCl requires a file descriptor that
  // was created in /tmp, since those created with shm_open() are not
  // mappable with PROT_EXEC.  Rather than requiring an extra IPC
  // round trip out of the sandbox, we create an FD here.
  base::SharedMemory memory_buffer;
  base::SharedMemoryCreateOptions options;
  options.size = 1;
  options.executable = true;
  if (!memory_buffer.Create(options)) {
    LOG(ERROR) << "Failed to allocate memory buffer";
    delete this;
    return;
  }
  nacl::FileDescriptor memory_fd;
  memory_fd.fd = dup(memory_buffer.handle().fd);
  if (memory_fd.fd < 0) {
    LOG(ERROR) << "Failed to dup() a file descriptor";
    delete this;
    return;
  }
  memory_fd.auto_close = true;
  handles_for_sel_ldr.push_back(memory_fd);
#endif

  Send(new NaClProcessMsg_Start(handles_for_sel_ldr));
  internal_->sockets_for_sel_ldr.clear();
}

bool NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED() << "Invalid message with type = " << msg.type();
  return false;
}
