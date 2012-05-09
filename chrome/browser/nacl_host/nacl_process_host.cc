// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_process_host.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "net/base/net_util.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "ipc/ipc_channel_posix.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "base/threading/thread.h"
#include "base/process_util.h"
#include "base/win/scoped_handle.h"
#include "chrome/browser/nacl_host/nacl_broker_service_win.h"
#include "chrome/common/nacl_debug_exception_handler_win.h"
#include "content/public/common/sandbox_init.h"
#endif

using content::BrowserThread;
using content::ChildProcessData;
using content::ChildProcessHost;

namespace {

#if defined(OS_WIN)
bool RunningOnWOW64() {
  return (base::win::OSInfo::GetInstance()->wow64_status() ==
          base::win::OSInfo::WOW64_ENABLED);
}
#endif

void SetCloseOnExec(nacl::Handle fd) {
#if defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFD);
  CHECK_NE(flags, -1);
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  CHECK_EQ(rc, 0);
#endif
}

bool ShareHandleToSelLdr(
    base::ProcessHandle processh,
    nacl::Handle sourceh,
    bool close_source,
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
    DLOG(ERROR) << "DuplicateHandle() failed";
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

}  // namespace

struct NaClProcessHost::NaClInternal {
  std::vector<nacl::Handle> sockets_for_renderer;
  std::vector<nacl::Handle> sockets_for_sel_ldr;
};

// -----------------------------------------------------------------------------

NaClProcessHost::NaClProcessHost(const GURL& manifest_url)
    : manifest_url_(manifest_url),
#if defined(OS_WIN)
      process_launched_by_broker_(false),
#elif defined(OS_LINUX)
      wait_for_nacl_gdb_(false),
#endif
      reply_msg_(NULL),
#if defined(OS_WIN)
      debug_exception_handler_requested_(false),
#endif
      internal_(new NaClInternal()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      enable_exception_handling_(false) {
  process_.reset(content::BrowserChildProcessHost::Create(
      content::PROCESS_TYPE_NACL_LOADER, this));

  // Set the display name so the user knows what plugin the process is running.
  // We aren't on the UI thread so getting the pref locale for language
  // formatting isn't possible, so IDN will be lost, but this is probably OK
  // for this use case.
  process_->SetName(net::FormatUrl(manifest_url_, std::string()));

  // We allow untrusted hardware exception handling to be enabled via
  // an env var for consistency with the standalone build of NaCl.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaClExceptionHandling) ||
      getenv("NACL_UNTRUSTED_EXCEPTION_HANDLING") != NULL) {
    enable_exception_handling_ = true;
  }
}

NaClProcessHost::~NaClProcessHost() {
  int exit_code;
  process_->GetTerminationStatus(&exit_code);
  std::string message =
      base::StringPrintf("NaCl process exited with status %i (0x%x)",
                         exit_code, exit_code);
  if (exit_code == 0) {
    LOG(INFO) << message;
  } else {
    LOG(ERROR) << message;
  }

  for (size_t i = 0; i < internal_->sockets_for_renderer.size(); i++) {
    if (nacl::Close(internal_->sockets_for_renderer[i]) != 0) {
      NOTREACHED() << "nacl::Close() failed";
    }
  }
  for (size_t i = 0; i < internal_->sockets_for_sel_ldr.size(); i++) {
    if (nacl::Close(internal_->sockets_for_sel_ldr[i]) != 0) {
      NOTREACHED() << "nacl::Close() failed";
    }
  }

  if (reply_msg_) {
    // The process failed to launch for some reason.
    // Don't keep the renderer hanging.
    reply_msg_->set_reply_error();
    chrome_render_message_filter_->Send(reply_msg_);
  }
#if defined(OS_WIN)
  if (process_launched_by_broker_) {
    NaClBrokerService::GetInstance()->OnLoaderDied();
  }
#endif
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

void NaClProcessHost::Launch(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    int socket_count,
    IPC::Message* reply_msg,
    scoped_refptr<ExtensionInfoMap> extension_info_map) {
  chrome_render_message_filter_ = chrome_render_message_filter;
  reply_msg_ = reply_msg;
  extension_info_map_ = extension_info_map;

  // Place an arbitrary limit on the number of sockets to limit
  // exposure in case the renderer is compromised.  We can increase
  // this if necessary.
  if (socket_count > 8) {
    delete this;
    return;
  }

  // Start getting the IRT open asynchronously while we launch the NaCl process.
  // We'll make sure this actually finished in StartWithLaunchedProcess, below.
  if (!NaClBrowser::GetInstance()->EnsureIrtAvailable()) {
    DLOG(ERROR) << "Cannot launch NaCl process after IRT file open failed";
    delete this;
    return;
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
    if (nacl::SocketPair(pair) == -1) {
      delete this;
      return;
    }
    internal_->sockets_for_renderer.push_back(pair[0]);
    internal_->sockets_for_sel_ldr.push_back(pair[1]);
    SetCloseOnExec(pair[0]);
    SetCloseOnExec(pair[1]);
  }

  // Launch the process
  if (!LaunchSelLdr()) {
    delete this;
  }
}

#if defined(OS_WIN)
void NaClProcessHost::OnChannelConnected(int32 peer_pid) {
  // Set process handle, if it was not set previously.
  // This is needed when NaCl process is launched with nacl-gdb.
  if (process_->GetData().handle == base::kNullProcessHandle) {
    base::ProcessHandle process;
    DCHECK(!CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kNaClGdb).empty());
    if (base::OpenProcessHandleWithAccess(
            peer_pid,
            base::kProcessAccessDuplicateHandle |
            base::kProcessAccessQueryInformation |
            base::kProcessAccessWaitForTermination,
            &process)) {
      process_->SetHandle(process);
      if (!StartWithLaunchedProcess()) {
        delete this;
        return;
      }
    } else {
      DLOG(ERROR) << "Failed to get process handle";
    }
  }
}
#else
void NaClProcessHost::OnChannelConnected(int32 peer_pid) {
}
#endif

#if defined(OS_WIN)
void NaClProcessHost::OnProcessLaunchedByBroker(base::ProcessHandle handle) {
  process_launched_by_broker_ = true;
  process_->SetHandle(handle);
  if (!StartWithLaunchedProcess())
    delete this;
}

void NaClProcessHost::OnDebugExceptionHandlerLaunchedByBroker(bool success) {
  IPC::Message* reply = attach_debug_exception_handler_reply_msg_.release();
  NaClProcessMsg_AttachDebugExceptionHandler::WriteReplyParams(reply, success);
  Send(reply);
}
#endif

// Needed to handle sync messages in OnMessageRecieved.
bool NaClProcessHost::Send(IPC::Message* msg) {
  return process_->Send(msg);
}

#if defined(OS_WIN)
scoped_ptr<CommandLine> NaClProcessHost::GetCommandForLaunchWithGdb(
    const FilePath& nacl_gdb,
    CommandLine* line) {
  CommandLine* cmd_line = new CommandLine(nacl_gdb);
  // We can't use PrependWrapper because our parameters contain spaces.
  cmd_line->AppendArg("--eval-command");
  const FilePath::StringType& irt_path =
      NaClBrowser::GetInstance()->GetIrtFilePath().value();
  cmd_line->AppendArgNative(FILE_PATH_LITERAL("nacl-irt ") + irt_path);
  FilePath manifest_path = GetManifestPath();
  if (!manifest_path.empty()) {
    cmd_line->AppendArg("--eval-command");
    cmd_line->AppendArgNative(FILE_PATH_LITERAL("nacl-manifest ") +
                              manifest_path.value());
  }
  cmd_line->AppendArg("--args");
  const CommandLine::StringVector& argv = line->argv();
  for (size_t i = 0; i < argv.size(); i++) {
    cmd_line->AppendArgNative(argv[i]);
  }
  return scoped_ptr<CommandLine>(cmd_line);
}
#elif defined(OS_LINUX)
namespace {
class NaClGdbWatchDelegate : public MessageLoopForIO::Watcher {
 public:
  // fd_write_ is used by nacl-gdb via /proc/browser_PID/fd/fd_write_
  NaClGdbWatchDelegate(int fd_read, int fd_write,
                       const base::Closure& reply)
      : fd_read_(fd_read),
        fd_write_(fd_write),
        reply_(reply) {}

  ~NaClGdbWatchDelegate() {
    if (HANDLE_EINTR(close(fd_read_)) != 0)
      DLOG(ERROR) << "close(fd_read_) failed";
    if (HANDLE_EINTR(close(fd_write_)) != 0)
      DLOG(ERROR) << "close(fd_write_) failed";
  }

  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}

 private:
  int fd_read_;
  int fd_write_;
  base::Closure reply_;
};

void NaClGdbWatchDelegate::OnFileCanReadWithoutBlocking(int fd) {
  char buf;
  if (HANDLE_EINTR(read(fd_read_, &buf, 1)) != 1 || buf != '\0')
    LOG(ERROR) << "Failed to sync with nacl-gdb";
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, reply_);
}
}  // namespace

bool NaClProcessHost::LaunchNaClGdb(base::ProcessId pid) {
  CommandLine::StringType nacl_gdb =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kNaClGdb);
  CommandLine::StringVector argv;
  // We don't support spaces inside arguments in --nacl-gdb switch.
  base::SplitString(nacl_gdb, static_cast<CommandLine::CharType>(' '), &argv);
  CommandLine cmd_line(argv);
  cmd_line.AppendArg("--eval-command");
  const FilePath::StringType& irt_path =
      NaClBrowser::GetInstance()->GetIrtFilePath().value();
  cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-irt ") + irt_path);
  FilePath manifest_path = GetManifestPath();
  if (!manifest_path.empty()) {
    cmd_line.AppendArg("--eval-command");
    cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-manifest ") +
                             manifest_path.value());
  }
  cmd_line.AppendArg("--eval-command");
  cmd_line.AppendArg("attach " + base::IntToString(pid));
  int fds[2];
  if (pipe(fds) != 0)
    return false;
  // Tell the debugger to send a byte to the writable end of the pipe.
  // We use a file descriptor in our process because the debugger will be
  // typically launched in a separate terminal, and a lot of terminals close all
  // file descriptors before launching external programs.
  cmd_line.AppendArg("--eval-command");
  cmd_line.AppendArg("dump binary value /proc/" +
                     base::IntToString(base::GetCurrentProcId()) +
                     "/fd/" + base::IntToString(fds[1]) + " (char)0");
  // wait on fds[0]
  // If the debugger crashes before attaching to the NaCl process, the user can
  // release resources by terminating the NaCl loader in Chrome Task Manager.
  nacl_gdb_watcher_delegate_.reset(
      new NaClGdbWatchDelegate(
          fds[0], fds[1],
          base::Bind(&NaClProcessHost::OnNaClGdbAttached,
                     weak_factory_.GetWeakPtr())));
  MessageLoopForIO::current()->WatchFileDescriptor(
      fds[0],
      true,
      MessageLoopForIO::WATCH_READ,
      &nacl_gdb_watcher_,
      nacl_gdb_watcher_delegate_.get());
  return base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL);
}

void NaClProcessHost::OnNaClGdbAttached() {
  wait_for_nacl_gdb_ = false;
  nacl_gdb_watcher_.StopWatchingFileDescriptor();
  nacl_gdb_watcher_delegate_.reset();
  OnProcessLaunched();
}
#endif

FilePath NaClProcessHost::GetManifestPath() {
  const Extension* extension = extension_info_map_->extensions()
      .GetExtensionOrAppByURL(ExtensionURLInfo(manifest_url_));
  if (extension != NULL && manifest_url_.SchemeIs(chrome::kExtensionScheme)) {
    std::string path = manifest_url_.path();
    TrimString(path, "/", &path);  // Remove first slash
    return extension->path().AppendASCII(path);
  }
  return FilePath();
}

bool NaClProcessHost::LaunchSelLdr() {
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
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

#if defined(OS_WIN)
  // On Windows 64-bit NaCl loader is called nacl64.exe instead of chrome.exe
  if (RunningOnWOW64()) {
    FilePath module_path;
    if (!PathService::Get(base::FILE_MODULE, &module_path))
      return false;
    exe_path = module_path.DirName().Append(chrome::kNaClAppName);
  }
#endif

  scoped_ptr<CommandLine> cmd_line(new CommandLine(exe_path));
  nacl::CopyNaClCommandLineArguments(cmd_line.get());

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kNaClLoaderProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  if (logging::DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  if (!nacl_loader_prefix.empty())
    cmd_line->PrependWrapper(nacl_loader_prefix);

  FilePath nacl_gdb = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kNaClGdb);
  if (!nacl_gdb.empty()) {
#if defined(OS_WIN)
    cmd_line->AppendSwitch(switches::kNoSandbox);
    scoped_ptr<CommandLine> gdb_cmd_line(
        GetCommandForLaunchWithGdb(nacl_gdb, cmd_line.get()));
    // We can't use process_->Launch() because OnProcessLaunched will be called
    // with process_->GetData().handle filled by handle of gdb process. This
    // handle will be used to duplicate handles for NaCl process and as
    // a result NaCl process will not be able to use them.
    //
    // So we don't fill process_->GetData().handle and wait for
    // OnChannelConnected to get handle of NaCl process from its pid. Then we
    // call OnProcessLaunched.
    return base::LaunchProcess(*gdb_cmd_line, base::LaunchOptions(), NULL);
#elif defined(OS_LINUX)
    wait_for_nacl_gdb_ = true;
#endif
  }

  // On Windows we might need to start the broker process to launch a new loader
#if defined(OS_WIN)
  if (RunningOnWOW64()) {
    return NaClBrokerService::GetInstance()->LaunchLoader(
        weak_factory_.GetWeakPtr(), channel_id);
  } else {
    process_->Launch(FilePath(), cmd_line.release());
  }
#elif defined(OS_POSIX)
  process_->Launch(nacl_loader_prefix.empty(),  // use_zygote
                   base::EnvironmentVector(),
                   cmd_line.release());
#endif

  return true;
}

bool NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClProcessHost, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_QueryKnownToValidate,
                        OnQueryKnownToValidate)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_SetKnownToValidate,
                        OnSetKnownToValidate)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClProcessMsg_AttachDebugExceptionHandler,
                                    OnAttachDebugExceptionHandler)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClProcessHost::OnProcessCrashed(int exit_code) {
  std::string message = base::StringPrintf(
      "NaCl process exited with status %i (0x%x)", exit_code, exit_code);
  LOG(ERROR) << message;
}

void NaClProcessHost::OnProcessLaunched() {
  if (!StartWithLaunchedProcess())
    delete this;
}

// The asynchronous attempt to get the IRT file open has completed.
void NaClProcessHost::IrtReady() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  if (!nacl_browser->IrtAvailable() || !SendStart()) {
    DLOG(ERROR) << "Cannot launch NaCl process";
    delete this;
  }
}

bool NaClProcessHost::ReplyToRenderer() {
  std::vector<nacl::FileDescriptor> handles_for_renderer;
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
      DLOG(ERROR) << "DuplicateHandle() failed";
      return false;
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
  // If we are on 64-bit Windows, the NaCl process's sandbox is
  // managed by a different process from the renderer's sandbox.  We
  // need to inform the renderer's sandbox about the NaCl process so
  // that the renderer can send handles to the NaCl process using
  // BrokerDuplicateHandle().
  if (RunningOnWOW64()) {
    if (!content::BrokerAddTargetPeer(process_->GetData().handle)) {
      DLOG(ERROR) << "Failed to add NaCl process PID";
      return false;
    }
  }
#endif

  ChromeViewHostMsg_LaunchNaCl::WriteReplyParams(
      reply_msg_, handles_for_renderer);
  chrome_render_message_filter_->Send(reply_msg_);
  chrome_render_message_filter_ = NULL;
  reply_msg_ = NULL;
  internal_->sockets_for_renderer.clear();
  return true;
}

bool NaClProcessHost::StartNaClExecution() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  nacl::NaClStartParams params;
  params.validation_cache_key =
      nacl_browser->validation_cache.GetValidationCacheKey();
  params.version = chrome::VersionInfo().CreateVersionString();
  params.enable_exception_handling = enable_exception_handling_;

  base::PlatformFile irt_file = nacl_browser->IrtFile();
  CHECK_NE(irt_file, base::kInvalidPlatformFileValue);

  const ChildProcessData& data = process_->GetData();
  for (size_t i = 0; i < internal_->sockets_for_sel_ldr.size(); i++) {
    if (!ShareHandleToSelLdr(data.handle,
                             internal_->sockets_for_sel_ldr[i], true,
                             &params.handles)) {
      return false;
    }
  }

  // Send over the IRT file handle.  We don't close our own copy!
  if (!ShareHandleToSelLdr(data.handle, irt_file, false, &params.handles))
    return false;

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
    DLOG(ERROR) << "Failed to allocate memory buffer";
    return false;
  }
  nacl::FileDescriptor memory_fd;
  memory_fd.fd = dup(memory_buffer.handle().fd);
  if (memory_fd.fd < 0) {
    DLOG(ERROR) << "Failed to dup() a file descriptor";
    return false;
  }
  memory_fd.auto_close = true;
  params.handles.push_back(memory_fd);
#endif

  process_->Send(new NaClProcessMsg_Start(params));

  internal_->sockets_for_sel_ldr.clear();
  return true;
}

bool NaClProcessHost::SendStart() {
  return ReplyToRenderer() && StartNaClExecution();
}

bool NaClProcessHost::StartWithLaunchedProcess() {
#if defined(OS_LINUX)
  if (wait_for_nacl_gdb_) {
    if (LaunchNaClGdb(base::GetProcId(process_->GetData().handle))) {
      // We will be called with wait_for_nacl_gdb_ = false once debugger is
      // attached to the program.
      return true;
    }
    DLOG(ERROR) << "Failed to launch debugger";
    // Continue execution without debugger.
  }
#endif

  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  if (nacl_browser->IrtAvailable())
    return SendStart();  // The IRT is already open.  Away we go.

  // We're waiting for the IRT to be open.
  return nacl_browser->MakeIrtAvailable(
      base::Bind(&NaClProcessHost::IrtReady, weak_factory_.GetWeakPtr()));
}

void NaClProcessHost::OnQueryKnownToValidate(const std::string& signature,
                                             bool* result) {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  *result = nacl_browser->validation_cache.QueryKnownToValidate(signature);
}

void NaClProcessHost::OnSetKnownToValidate(const std::string& signature) {
  NaClBrowser::GetInstance()->validation_cache.SetKnownToValidate(signature);
}

#if defined(OS_WIN)
void NaClProcessHost::OnAttachDebugExceptionHandler(const std::string& info,
                                                    IPC::Message* reply_msg) {
  if (!AttachDebugExceptionHandler(info, reply_msg)) {
    // Send failure message.
    NaClProcessMsg_AttachDebugExceptionHandler::WriteReplyParams(reply_msg,
                                                                 false);
    Send(reply_msg);
  }
}

bool NaClProcessHost::AttachDebugExceptionHandler(const std::string& info,
                                                  IPC::Message* reply_msg) {
  if (!enable_exception_handling_) {
    DLOG(ERROR) <<
        "Exception handling requested by NaCl process when not enabled";
    return false;
  }
  if (debug_exception_handler_requested_) {
    // The NaCl process should not request this multiple times.
    DLOG(ERROR) << "Multiple AttachDebugExceptionHandler requests received";
    return false;
  }
  debug_exception_handler_requested_ = true;

  base::ProcessId nacl_pid = base::GetProcId(process_->GetData().handle);
  base::win::ScopedHandle process_handle;
  // We cannot use process_->GetData().handle because it does not have
  // the necessary access rights.  We open the new handle here rather
  // than in the NaCl broker process in case the NaCl loader process
  // dies before the NaCl broker process receives the message we send.
  // The debug exception handler uses DebugActiveProcess() to attach,
  // but this takes a PID.  We need to prevent the NaCl loader's PID
  // from being reused before DebugActiveProcess() is called, and
  // holding a process handle open achieves this.
  if (!base::OpenProcessHandleWithAccess(
           nacl_pid,
           base::kProcessAccessQueryInformation |
           base::kProcessAccessSuspendResume |
           base::kProcessAccessTerminate |
           base::kProcessAccessVMOperation |
           base::kProcessAccessVMRead |
           base::kProcessAccessVMWrite |
           base::kProcessAccessWaitForTermination,
           process_handle.Receive())) {
    LOG(ERROR) << "Failed to get process handle";
    return false;
  }

  attach_debug_exception_handler_reply_msg_.reset(reply_msg);
  // If the NaCl loader is 64-bit, the process running its debug
  // exception handler must be 64-bit too, so we use the 64-bit NaCl
  // broker process for this.  Otherwise, on a 32-bit system, we use
  // the 32-bit browser process to run the debug exception handler.
  if (RunningOnWOW64()) {
    return NaClBrokerService::GetInstance()->LaunchDebugExceptionHandler(
               weak_factory_.GetWeakPtr(), nacl_pid, process_handle, info);
  } else {
    NaClStartDebugExceptionHandlerThread(
        process_handle.Take(), info,
        base::MessageLoopProxy::current(),
        base::Bind(&NaClProcessHost::OnDebugExceptionHandlerLaunchedByBroker,
                   weak_factory_.GetWeakPtr()));
    return true;
  }
}
#endif
