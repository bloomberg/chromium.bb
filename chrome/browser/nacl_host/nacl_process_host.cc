// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_process_host.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_process_type.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/process_type.h"
#include "extensions/common/constants.h"
#include "extensions/common/url_pattern.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "net/base/net_util.h"
#include "net/base/tcp_listen_socket.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "ipc/ipc_channel_posix.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "base/process_util.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "chrome/browser/nacl_host/nacl_broker_service_win.h"
#include "chrome/common/nacl_debug_exception_handler_win.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#endif

using content::BrowserThread;
using content::ChildProcessData;
using content::ChildProcessHost;
using ppapi::proxy::SerializedHandle;

namespace {

#if defined(OS_WIN)
bool RunningOnWOW64() {
  return (base::win::OSInfo::GetInstance()->wow64_status() ==
          base::win::OSInfo::WOW64_ENABLED);
}

// NOTE: changes to this class need to be reviewed by the security team.
class NaClSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  NaClSandboxedProcessLauncherDelegate() {}
  virtual ~NaClSandboxedProcessLauncherDelegate() {}

  virtual void PostSpawnTarget(base::ProcessHandle process) {
#if !defined(NACL_WIN64)
    // For Native Client sel_ldr processes on 32-bit Windows, reserve 1 GB of
    // address space to prevent later failure due to address space fragmentation
    // from .dll loading. The NaCl process will attempt to locate this space by
    // scanning the address space using VirtualQuery.
    // TODO(bbudge) Handle the --no-sandbox case.
    // http://code.google.com/p/nativeclient/issues/detail?id=2131
    const SIZE_T kOneGigabyte = 1 << 30;
    void* nacl_mem = VirtualAllocEx(process,
                                    NULL,
                                    kOneGigabyte,
                                    MEM_RESERVE,
                                    PAGE_NOACCESS);
    if (!nacl_mem) {
      DLOG(WARNING) << "Failed to reserve address space for Native Client";
    }
#endif  // !defined(NACL_WIN64)
  }
};

#endif  // OS_WIN

void SetCloseOnExec(NaClHandle fd) {
#if defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFD);
  CHECK_NE(flags, -1);
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  CHECK_EQ(rc, 0);
#endif
}

bool ShareHandleToSelLdr(
    base::ProcessHandle processh,
    NaClHandle sourceh,
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

ppapi::PpapiPermissions GetNaClPermissions(uint32 permission_bits) {
  // Only allow NaCl plugins to request certain permissions. We don't want
  // a compromised renderer to be able to start a nacl plugin with e.g. Flash
  // permissions which may expand the surface area of the sandbox.
  uint32 masked_bits = permission_bits & ppapi::PERMISSION_DEV;
  return ppapi::PpapiPermissions::GetForCommandLine(masked_bits);
}

}  // namespace

struct NaClProcessHost::NaClInternal {
  NaClHandle socket_for_renderer;
  NaClHandle socket_for_sel_ldr;

  NaClInternal()
    : socket_for_renderer(NACL_INVALID_HANDLE),
      socket_for_sel_ldr(NACL_INVALID_HANDLE) { }
};

// -----------------------------------------------------------------------------

NaClProcessHost::PluginListener::PluginListener(NaClProcessHost* host)
    : host_(host) {
}

bool NaClProcessHost::PluginListener::OnMessageReceived(
    const IPC::Message& msg) {
  return host_->OnUntrustedMessageForwarded(msg);
}

NaClProcessHost::NaClProcessHost(const GURL& manifest_url,
                                 int render_view_id,
                                 uint32 permission_bits,
                                 bool uses_irt,
                                 bool off_the_record)
    : manifest_url_(manifest_url),
      permissions_(GetNaClPermissions(permission_bits)),
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
      enable_exception_handling_(false),
      enable_debug_stub_(false),
      uses_irt_(uses_irt),
      off_the_record_(off_the_record),
      ALLOW_THIS_IN_INITIALIZER_LIST(ipc_plugin_listener_(this)),
      render_view_id_(render_view_id) {
  process_.reset(content::BrowserChildProcessHost::Create(
      PROCESS_TYPE_NACL_LOADER, this));

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
  enable_debug_stub_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNaClDebug);
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

  if (internal_->socket_for_renderer != NACL_INVALID_HANDLE) {
    if (NaClClose(internal_->socket_for_renderer) != 0) {
      NOTREACHED() << "NaClClose() failed";
    }
  }

  if (internal_->socket_for_sel_ldr != NACL_INVALID_HANDLE) {
    if (NaClClose(internal_->socket_for_sel_ldr) != 0) {
      NOTREACHED() << "NaClClose() failed";
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
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.nacl-gdb",
      !cmd->GetSwitchValuePath(switches::kNaClGdb).empty());
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.nacl-gdb-script",
      !cmd->GetSwitchValuePath(switches::kNaClGdbScript).empty());
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.enable-nacl-debug",
      cmd->HasSwitch(switches::kEnableNaClDebug));
  NaClBrowser::GetInstance()->SetDebugPatterns(
      cmd->GetSwitchValueASCII(switches::kNaClDebugMask));
}

void NaClProcessHost::Launch(
    ChromeRenderMessageFilter* chrome_render_message_filter,
    IPC::Message* reply_msg,
    scoped_refptr<ExtensionInfoMap> extension_info_map) {
  chrome_render_message_filter_ = chrome_render_message_filter;
  reply_msg_ = reply_msg;
  extension_info_map_ = extension_info_map;

  // Start getting the IRT open asynchronously while we launch the NaCl process.
  // We'll make sure this actually finished in StartWithLaunchedProcess, below.
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  nacl_browser->EnsureAllResourcesAvailable();
  if (!nacl_browser->IsOk()) {
    LOG(ERROR) << "NaCl process launch failed: could not find all the "
        "resources needed to launch the process";
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

  NaClHandle pair[2];
  // Create a connected socket
  if (NaClSocketPair(pair) == -1) {
    LOG(ERROR) << "NaCl process launch failed: could not create a socket pair";
    delete this;
    return;
  }
  internal_->socket_for_renderer = pair[0];
  internal_->socket_for_sel_ldr = pair[1];
  SetCloseOnExec(pair[0]);
  SetCloseOnExec(pair[1]);

  // Launch the process
  if (!LaunchSelLdr()) {
    delete this;
  }
}

#if defined(OS_WIN)
void NaClProcessHost::OnChannelConnected(int32 peer_pid) {
  // Set process handle, if it was not set previously.
  // This is needed when NaCl process is launched with nacl-gdb.
  if (!CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kNaClGdb).empty()) {
    base::ProcessHandle process;
    DCHECK(process_->GetData().handle == base::kNullProcessHandle);
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
      LOG(ERROR) << "Failed to get process handle";
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
    const base::FilePath& nacl_gdb,
    CommandLine* line) {
  CommandLine* cmd_line = new CommandLine(nacl_gdb);
  // We can't use PrependWrapper because our parameters contain spaces.
  cmd_line->AppendArg("--eval-command");
  const base::FilePath::StringType& irt_path =
      NaClBrowser::GetInstance()->GetIrtFilePath().value();
  cmd_line->AppendArgNative(FILE_PATH_LITERAL("nacl-irt ") + irt_path);
  base::FilePath manifest_path = GetManifestPath();
  if (!manifest_path.empty()) {
    cmd_line->AppendArg("--eval-command");
    cmd_line->AppendArgNative(FILE_PATH_LITERAL("nacl-manifest ") +
                              manifest_path.value());
  }
  base::FilePath script = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kNaClGdbScript);
  if (!script.empty()) {
    cmd_line->AppendArg("--command");
    cmd_line->AppendArgNative(script.value());
  }
  cmd_line->AppendArg("--args");
  const CommandLine::StringVector& argv = line->argv();
  for (size_t i = 0; i < argv.size(); i++) {
    cmd_line->AppendArgNative(argv[i]);
  }
  return scoped_ptr<CommandLine>(cmd_line);
}
#elif defined(OS_LINUX)
class NaClProcessHost::NaClGdbWatchDelegate
    : public MessageLoopForIO::Watcher {
 public:
  // fd_write_ is used by nacl-gdb via /proc/browser_PID/fd/fd_write_
  NaClGdbWatchDelegate(int fd_read, int fd_write,
                       const base::Closure& reply)
      : fd_read_(fd_read),
        fd_write_(fd_write),
        reply_(reply) {}

  virtual ~NaClGdbWatchDelegate() {
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

void NaClProcessHost::NaClGdbWatchDelegate::OnFileCanReadWithoutBlocking(
    int fd) {
  char buf;
  if (HANDLE_EINTR(read(fd_read_, &buf, 1)) != 1 || buf != '\0')
    LOG(ERROR) << "Failed to sync with nacl-gdb";
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, reply_);
}

bool NaClProcessHost::LaunchNaClGdb(base::ProcessId pid) {
  CommandLine::StringType nacl_gdb =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kNaClGdb);
  CommandLine::StringVector argv;
  // We don't support spaces inside arguments in --nacl-gdb switch.
  base::SplitString(nacl_gdb, static_cast<CommandLine::CharType>(' '), &argv);
  CommandLine cmd_line(argv);
  cmd_line.AppendArg("--eval-command");
  const base::FilePath::StringType& irt_path =
      NaClBrowser::GetInstance()->GetIrtFilePath().value();
  cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-irt ") + irt_path);
  base::FilePath manifest_path = GetManifestPath();
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
  base::FilePath script = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kNaClGdbScript);
  if (!script.empty()) {
    cmd_line.AppendArg("--command");
    cmd_line.AppendArgNative(script.value());
  }
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

base::FilePath NaClProcessHost::GetManifestPath() {
  const extensions::Extension* extension = extension_info_map_->extensions()
      .GetExtensionOrAppByURL(ExtensionURLInfo(manifest_url_));
  if (extension != NULL &&
      manifest_url_.SchemeIs(extensions::kExtensionScheme)) {
    std::string path = manifest_url_.path();
    TrimString(path, "/", &path);  // Remove first slash
    return extension->path().AppendASCII(path);
  }
  return base::FilePath();
}

bool NaClProcessHost::LaunchSelLdr() {
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty()) {
    LOG(ERROR) << "NaCl process launch failed: could not create channel";
    return false;
  }

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

  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

#if defined(OS_WIN)
  // On Windows 64-bit NaCl loader is called nacl64.exe instead of chrome.exe
  if (RunningOnWOW64()) {
    base::FilePath module_path;
    if (!PathService::Get(base::FILE_MODULE, &module_path)) {
      LOG(ERROR) << "NaCl process launch failed: could not resolve module";
      return false;
    }
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

  base::FilePath nacl_gdb =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(switches::kNaClGdb);
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
    if (!NaClBrokerService::GetInstance()->LaunchLoader(
            weak_factory_.GetWeakPtr(), channel_id)) {
      LOG(ERROR) << "NaCl process launch failed: broker service did not launch "
          "process";
      return false;
    }
  } else {
    process_->Launch(new NaClSandboxedProcessLauncherDelegate,
                     cmd_line.release());
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
    IPC_MESSAGE_HANDLER(NaClProcessHostMsg_PpapiChannelCreated,
                        OnPpapiChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClProcessHost::OnProcessLaunched() {
  if (!StartWithLaunchedProcess())
    delete this;
}

// Called when the NaClBrowser singleton has been fully initialized.
void NaClProcessHost::OnResourcesReady() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  if (!nacl_browser->IsReady()) {
    LOG(ERROR) << "NaCl process launch failed: could not acquire shared "
        "resources needed by NaCl";
    delete this;
  } else if (!SendStart()) {
    delete this;
  }
}

bool NaClProcessHost::ReplyToRenderer(
    const IPC::ChannelHandle& channel_handle) {
  nacl::FileDescriptor handle_for_renderer;
#if defined(OS_WIN)
  // Copy the handle into the renderer process.
  HANDLE handle_in_renderer;
  if (!DuplicateHandle(base::GetCurrentProcessHandle(),
                       reinterpret_cast<HANDLE>(
                           internal_->socket_for_renderer),
                       chrome_render_message_filter_->peer_handle(),
                       &handle_in_renderer,
                       0,  // Unused given DUPLICATE_SAME_ACCESS.
                       FALSE,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    LOG(ERROR) << "DuplicateHandle() failed";
    return false;
  }
  handle_for_renderer = reinterpret_cast<nacl::FileDescriptor>(
      handle_in_renderer);
#else
  // No need to dup the imc_handle - we don't pass it anywhere else so
  // it cannot be closed.
  nacl::FileDescriptor imc_handle;
  imc_handle.fd = internal_->socket_for_renderer;
  imc_handle.auto_close = true;
  handle_for_renderer = imc_handle;
#endif

#if defined(OS_WIN)
  // If we are on 64-bit Windows, the NaCl process's sandbox is
  // managed by a different process from the renderer's sandbox.  We
  // need to inform the renderer's sandbox about the NaCl process so
  // that the renderer can send handles to the NaCl process using
  // BrokerDuplicateHandle().
  if (RunningOnWOW64()) {
    if (!content::BrokerAddTargetPeer(process_->GetData().handle)) {
      LOG(ERROR) << "Failed to add NaCl process PID";
      return false;
    }
  }
#endif

  const ChildProcessData& data = process_->GetData();
  ChromeViewHostMsg_LaunchNaCl::WriteReplyParams(
      reply_msg_, handle_for_renderer,
      channel_handle, base::GetProcId(data.handle), data.id);
  chrome_render_message_filter_->Send(reply_msg_);
  chrome_render_message_filter_ = NULL;
  reply_msg_ = NULL;
  internal_->socket_for_renderer = NACL_INVALID_HANDLE;
  return true;
}

// TCP port we chose for NaCl debug stub. It can be any other number.
static const int kDebugStubPort = 4014;

#if defined(OS_POSIX)
SocketDescriptor NaClProcessHost::GetDebugStubSocketHandle() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  SocketDescriptor s;
  // We allocate currently unused TCP port for debug stub tests. The port
  // number is passed to the test via debug stub port listener.
  if (nacl_browser->HasGdbDebugStubPortListener()) {
    int port;
    s = net::TCPListenSocket::CreateAndBindAnyPort("127.0.0.1", &port);
    if (s != net::TCPListenSocket::kInvalidSocket) {
      nacl_browser->FireGdbDebugStubPortOpened(port);
    }
  } else {
    s = net::TCPListenSocket::CreateAndBind("127.0.0.1", kDebugStubPort);
  }
  if (s == net::TCPListenSocket::kInvalidSocket) {
    LOG(ERROR) << "failed to open socket for debug stub";
    return net::TCPListenSocket::kInvalidSocket;
  }
  if (listen(s, 1)) {
    LOG(ERROR) << "listen() failed on debug stub socket";
    if (HANDLE_EINTR(close(s)) < 0)
      PLOG(ERROR) << "failed to close debug stub socket";
    return net::TCPListenSocket::kInvalidSocket;
  }
  return s;
}
#endif

bool NaClProcessHost::StartNaClExecution() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  nacl::NaClStartParams params;
  params.validation_cache_enabled = nacl_browser->ValidationCacheIsEnabled();
  params.validation_cache_key = nacl_browser->GetValidationCacheKey();
  params.version = chrome::VersionInfo().CreateVersionString();
  params.enable_exception_handling = enable_exception_handling_;
  params.enable_debug_stub = enable_debug_stub_ &&
      NaClBrowser::GetInstance()->URLMatchesDebugPatterns(manifest_url_);
  // Enable PPAPI proxy channel creation only for renderer processes.
  params.enable_ipc_proxy = enable_ppapi_proxy();
  params.uses_irt = uses_irt_;

  const ChildProcessData& data = process_->GetData();
  if (!ShareHandleToSelLdr(data.handle,
                           internal_->socket_for_sel_ldr, true,
                           &params.handles)) {
    return false;
  }

  if (params.uses_irt) {
    base::PlatformFile irt_file = nacl_browser->IrtFile();
    CHECK_NE(irt_file, base::kInvalidPlatformFileValue);

    // Send over the IRT file handle.  We don't close our own copy!
    if (!ShareHandleToSelLdr(data.handle, irt_file, false, &params.handles))
      return false;
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

#if defined(OS_POSIX)
  if (params.enable_debug_stub) {
    SocketDescriptor server_bound_socket = GetDebugStubSocketHandle();
    if (server_bound_socket != net::TCPListenSocket::kInvalidSocket) {
      params.debug_stub_server_bound_socket =
          nacl::FileDescriptor(server_bound_socket, true);
    }
  }
#endif

  process_->Send(new NaClProcessMsg_Start(params));

  internal_->socket_for_sel_ldr = NACL_INVALID_HANDLE;
  return true;
}

bool NaClProcessHost::SendStart() {
  if (!enable_ppapi_proxy()) {
    if (!ReplyToRenderer(IPC::ChannelHandle()))
      return false;
  }
  return StartNaClExecution();
}

// This method is called when NaClProcessHostMsg_PpapiChannelCreated is
// received or PpapiHostMsg_ChannelCreated is forwarded by our plugin
// listener.
void NaClProcessHost::OnPpapiChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  // Only renderer processes should create a channel.
  DCHECK(enable_ppapi_proxy());
  // If the proxy channel is null, this must be the initial NaCl-Browser IPC
  // channel.
  if (!ipc_proxy_channel_.get()) {
    DCHECK_EQ(PROCESS_TYPE_NACL_LOADER, process_->GetData().process_type);

    ipc_proxy_channel_.reset(
        new IPC::ChannelProxy(channel_handle,
                              IPC::Channel::MODE_CLIENT,
                              &ipc_plugin_listener_,
                              base::MessageLoopProxy::current()));
    // Create the browser ppapi host and enable PPAPI message dispatching to the
    // browser process.
    ppapi_host_.reset(content::BrowserPpapiHost::CreateExternalPluginProcess(
        ipc_proxy_channel_.get(),  // sender
        permissions_,
        process_->GetData().handle,
        ipc_proxy_channel_.get(),
        chrome_render_message_filter_->GetHostResolver(),
        chrome_render_message_filter_->render_process_id(),
        render_view_id_));

    // Send a message to create the NaCl-Renderer channel. The handle is just
    // a place holder.
    ipc_proxy_channel_->Send(
        new PpapiMsg_CreateNaClChannel(
            chrome_render_message_filter_->render_process_id(),
            permissions_,
            chrome_render_message_filter_->off_the_record(),
            SerializedHandle(SerializedHandle::CHANNEL_HANDLE,
                             IPC::InvalidPlatformFileForTransit())));
  } else if (reply_msg_) {
    // Otherwise, this must be a renderer channel.
    ReplyToRenderer(channel_handle);
  } else {
    // Attempt to open more than 1 renderer channel is not supported.
    // Shut down the NaCl process.
    process_->GetHost()->ForceShutdown();
  }
}

bool NaClProcessHost::OnUntrustedMessageForwarded(const IPC::Message& msg) {
  // Handle messages that have been forwarded from our PluginListener.
  // These messages come from untrusted code so should be handled with care.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ChannelCreated,
                        OnPpapiChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

  if (nacl_browser->IsReady()) {
    return SendStart();
  } else if (nacl_browser->IsOk()) {
    nacl_browser->WaitForResources(
        base::Bind(&NaClProcessHost::OnResourcesReady,
                   weak_factory_.GetWeakPtr()));
    return true;
  } else {
    LOG(ERROR) << "NaCl process failed to launch: previously failed to acquire "
        "shared resources";
    return false;
  }
}

void NaClProcessHost::OnQueryKnownToValidate(const std::string& signature,
                                             bool* result) {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  *result = nacl_browser->QueryKnownToValidate(signature, off_the_record_);
}

void NaClProcessHost::OnSetKnownToValidate(const std::string& signature) {
  NaClBrowser::GetInstance()->SetKnownToValidate(signature, off_the_record_);
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
  if (!enable_exception_handling_ && !enable_debug_stub_) {
    DLOG(ERROR) <<
        "Debug exception handler requested by NaCl process when not enabled";
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
           base::kProcessAccessDuplicateHandle |
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
