// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_process_host.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/nacl_cmd_line.h"
#include "chrome/common/nacl_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/shared/imc/nacl_imc.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "ipc/ipc_channel_posix.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "base/threading/thread.h"
#include "base/process_util.h"
#include "chrome/browser/nacl_host/nacl_broker_service_win.h"
#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"
#endif

using content::BrowserThread;
using content::ChildProcessData;
using content::ChildProcessHost;

#if defined(OS_WIN)
class NaClProcessHost::DebugContext
  : public base::RefCountedThreadSafe<NaClProcessHost::DebugContext> {
 public:
  DebugContext()
      : can_send_start_msg_(false),
        child_process_host_(NULL) {
  }

  ~DebugContext() {
  }

  void AttachDebugger(int pid, base::ProcessHandle process);

  // 6 methods below must be called on Browser::IO thread.
  void SetStartMessage(IPC::Message* start_msg);
  void SetChildProcessHost(content::ChildProcessHost* child_process_host);
  void SetDebugThread(base::Thread* thread_);

  // Start message is sent from 2 flows of execution. The first flow is
  // NaClProcessHost::SendStart. The second flow is
  // NaClProcessHost::OnChannelConnected and
  // NaClProcessHost::DebugContext::AttachThread. The message itself is created
  // by first flow. But the moment it can be sent is determined by second flow.
  // So first flow executes SetStartMessage and SendStartMessage while second
  // flow uses AllowAndSendStartMessage to either send potentially pending
  // start message or set the flag that allows the first flow to do this.

  // Clears the flag that prevents sending start message.
  void AllowToSendStartMsg();
  // Send start message to the NaCl process or do nothing if message is not
  // set or not allowed to be send. If message is sent, it is cleared and
  // repeated calls do nothing.
  void SendStartMessage();
  // Clear the flag that prevents further sending start message and send start
  // message if it is set.
  void AllowAndSendStartMessage();
 private:
  void StopThread();
  // These 4 fields are accessed only from Browser::IO thread.
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<IPC::Message> start_msg_;
  // Debugger is attached or exception handling is not switched on.
  // This means that start message can be sent to the NaCl process.
  bool can_send_start_msg_;
  content::ChildProcessHost* child_process_host_;
};

void NaClProcessHost::DebugContext::AttachDebugger(
    int pid, base::ProcessHandle process) {
  BOOL attached;
  DWORD exit_code;
  attached = DebugActiveProcess(pid);
  if (!attached) {
    LOG(ERROR) << "Failed to connect to the process";
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &NaClProcessHost::DebugContext::AllowAndSendStartMessage, this));
  if (attached) {
    // debug the process
    NaClDebugLoop(process, &exit_code);
    base::CloseProcessHandle(process);
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NaClProcessHost::DebugContext::StopThread, this));
}

void NaClProcessHost::DebugContext::SetStartMessage(IPC::Message* start_msg) {
  start_msg_.reset(start_msg);
}

void NaClProcessHost::DebugContext::SetChildProcessHost(
    content::ChildProcessHost* child_process_host) {
  child_process_host_ = child_process_host;
}

void NaClProcessHost::DebugContext::SetDebugThread(base::Thread* thread) {
  thread_.reset(thread);
}

void NaClProcessHost::DebugContext::AllowToSendStartMsg() {
  can_send_start_msg_ = true;
}

void NaClProcessHost::DebugContext::SendStartMessage() {
  if (start_msg_.get() && can_send_start_msg_) {
    if (child_process_host_) {
      if (!child_process_host_->Send(start_msg_.release())) {
        LOG(ERROR) << "Failed to send start message";
      }
    }
  }
}

void NaClProcessHost::DebugContext::AllowAndSendStartMessage() {
  AllowToSendStartMsg();
  SendStartMessage();
}

void NaClProcessHost::DebugContext::StopThread() {
  thread_->Stop();
  thread_.reset();
}
#endif

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

  // Path to IRT. Available even before IRT is loaded.
  const FilePath& GetIrtFilePath();

 private:
  base::PlatformFile irt_platform_file_;

  FilePath irt_filepath_;

  friend struct DefaultSingletonTraits<NaClBrowser>;

  NaClBrowser()
      : irt_platform_file_(base::kInvalidPlatformFileValue),
        irt_filepath_() {
    InitIrtFilePath();
  }

  ~NaClBrowser() {
    if (irt_platform_file_ != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(irt_platform_file_);
  }

  void InitIrtFilePath();

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

#if defined(OS_WIN)
static bool RunningOnWOW64() {
  return (base::win::OSInfo::GetInstance()->wow64_status() ==
          base::win::OSInfo::WOW64_ENABLED);
}
#endif

NaClProcessHost::NaClProcessHost(const std::wstring& url)
    :
#if defined(OS_WIN)
      process_launched_by_broker_(false),
#endif
      reply_msg_(NULL),
      internal_(new NaClInternal()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      enable_exception_handling_(false) {
  process_.reset(content::BrowserChildProcessHost::Create(
      content::PROCESS_TYPE_NACL_LOADER, this));
  process_->SetName(WideToUTF16Hack(url));

  // We allow untrusted hardware exception handling to be enabled via
  // an env var for consistency with the standalone build of NaCl.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaClExceptionHandling) ||
      getenv("NACL_UNTRUSTED_EXCEPTION_HANDLING") != NULL) {
    enable_exception_handling_ = true;
#if defined(OS_WIN)
    if (!RunningOnWOW64()) {
      debug_context_ = new DebugContext();
    }
#endif
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
#if defined(OS_WIN)
  if (process_launched_by_broker_) {
    NaClBrokerService::GetInstance()->OnLoaderDied();
  }
  if (debug_context_ != NULL) {
    debug_context_->SetChildProcessHost(NULL);
  }
#endif
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
  // We'll make sure this actually finished in OnProcessLaunched, below.
  if (!NaClBrowser::GetInstance()->EnsureIrtAvailable()) {
    LOG(ERROR) << "Cannot launch NaCl process after IRT file open failed";
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

scoped_ptr<CommandLine> NaClProcessHost::LaunchWithNaClGdb(
    const FilePath& nacl_gdb,
    CommandLine* line,
    const FilePath& manifest_path) {
  CommandLine* cmd_line = new CommandLine(nacl_gdb);
  // We can't use PrependWrapper because our parameters contain spaces.
  cmd_line->AppendArg("--eval-command");
  const FilePath::StringType& irt_path =
      NaClBrowser::GetInstance()->GetIrtFilePath().value();
  cmd_line->AppendArgNative(FILE_PATH_LITERAL("nacl-irt ") + irt_path);
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
    GURL manifest_url = GURL(process_->GetData().name);
    FilePath manifest_path;
    const Extension* extension = extension_info_map_->extensions().
        GetExtensionOrAppByURL(ExtensionURLInfo(manifest_url));
    if (extension != NULL && manifest_url.SchemeIs(chrome::kExtensionScheme)) {
      std::string path = manifest_url.path();
      TrimString(path, "/", &path);  // Remove first slash
      manifest_path = extension->path().AppendASCII(path);
    }
    cmd_line->AppendSwitch(switches::kNoSandbox);
    scoped_ptr<CommandLine> gdb_cmd_line(
        LaunchWithNaClGdb(nacl_gdb, cmd_line.get(), manifest_path));
    // We can't use process_->Launch() because OnProcessLaunched will be called
    // with process_->GetData().handle filled by handle of gdb process. This
    // handle will be used to duplicate handles for NaCl process and as
    // a result NaCl process will not be able to use them.
    //
    // So we don't fill process_->GetData().handle and wait for
    // OnChannelConnected to get handle of NaCl process from its pid. Then we
    // call OnProcessLaunched.
    return base::LaunchProcess(*gdb_cmd_line, base::LaunchOptions(), NULL);
  }

  // On Windows we might need to start the broker process to launch a new loader
#if defined(OS_WIN)
  if (RunningOnWOW64()) {
    return NaClBrokerService::GetInstance()->LaunchLoader(
        this, ASCIIToWide(channel_id));
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

void NaClProcessHost::OnProcessLaunchedByBroker(base::ProcessHandle handle) {
#if defined(OS_WIN)
  process_launched_by_broker_ = true;
#endif
  process_->SetHandle(handle);
  OnProcessLaunched();
}

void NaClProcessHost::OnProcessCrashed(int exit_code) {
  std::string message = base::StringPrintf(
      "NaCl process exited with status %i (0x%x)", exit_code, exit_code);
  LOG(ERROR) << message;
}

namespace {

// Determine the name of the IRT file based on the architecture.

#define NACL_IRT_FILE_NAME(arch_string) \
  (FILE_PATH_LITERAL("nacl_irt_")       \
   FILE_PATH_LITERAL(arch_string)       \
   FILE_PATH_LITERAL(".nexe"))

const FilePath::StringType NaClIrtName() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  bool is64 = true;
#elif defined(OS_WIN)
  bool is64 = RunningOnWOW64();
#else
  bool is64 = false;
#endif
  return is64 ? NACL_IRT_FILE_NAME("x86_64") : NACL_IRT_FILE_NAME("x86_32");
#elif defined(ARCH_CPU_ARMEL)
  // TODO(mcgrathr): Eventually we'll need to distinguish arm32 vs thumb2.
  // That may need to be based on the actual nexe rather than a static
  // choice, which would require substantial refactoring.
  return NACL_IRT_FILE_NAME("arm");
#else
#error Add support for your architecture to NaCl IRT file selection
#endif
}

}  // namespace

void NaClBrowser::InitIrtFilePath() {
  // Allow the IRT library to be overridden via an environment
  // variable.  This allows the NaCl/Chromium integration bot to
  // specify a newly-built IRT rather than using a prebuilt one
  // downloaded via Chromium's DEPS file.  We use the same environment
  // variable that the standalone NaCl PPAPI plugin accepts.
  const char* irt_path_var = getenv("NACL_IRT_LIBRARY");
  if (irt_path_var != NULL) {
    FilePath::StringType path_string(
        irt_path_var, const_cast<const char*>(strchr(irt_path_var, '\0')));
    irt_filepath_ = FilePath(path_string);
  } else {
    FilePath plugin_dir;
    if (!PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &plugin_dir)) {
      LOG(ERROR) << "Failed to locate the plugins directory";
      return;
    }

    irt_filepath_ = plugin_dir.Append(NaClIrtName());
  }
}

const FilePath& NaClBrowser::GetIrtFilePath() {
  return irt_filepath_;
}

// This only ever runs on the BrowserThread::FILE thread.
// If multiple tasks are posted, the later ones are no-ops.
void NaClBrowser::OpenIrtLibraryFile() {
  if (irt_platform_file_ != base::kInvalidPlatformFileValue)
    // We've already run.
    return;

  base::PlatformFileError error_code;
  irt_platform_file_ = base::CreatePlatformFile(irt_filepath_,
                                                base::PLATFORM_FILE_OPEN |
                                                base::PLATFORM_FILE_READ,
                                                NULL,
                                                &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Failed to open NaCl IRT file \""
               << irt_filepath_.LossyDisplayName()
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
    if (!nacl_browser->MakeIrtAvailable(
            base::Bind(&NaClProcessHost::IrtReady,
                       weak_factory_.GetWeakPtr())))
      delete this;
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
      OnProcessLaunched();
    } else {
      LOG(ERROR) << "Failed to get process handle";
    }
  }
  if (debug_context_ == NULL) {
    return;
  }
  // Start new thread for debug loop
  debug_context_->SetChildProcessHost(process_->GetHost());
  // We can't use process_->GetData().handle because it doesn't have necessary
  // access rights.
  base::ProcessHandle process;
  if (!base::OpenProcessHandleWithAccess(
           peer_pid,
           base::kProcessAccessQueryInformation |
           base::kProcessAccessSuspendResume |
           base::kProcessAccessTerminate |
           base::kProcessAccessVMOperation |
           base::kProcessAccessVMRead |
           base::kProcessAccessVMWrite |
           base::kProcessAccessWaitForTermination,
           &process)) {
    LOG(ERROR) << "Failed to open the process";
    debug_context_->AllowAndSendStartMessage();
    return;
  }
  base::Thread* dbg_thread = new base::Thread("Debug thread");
  if (!dbg_thread->Start()) {
    LOG(ERROR) << "Debug thread not started";
    debug_context_->AllowAndSendStartMessage();
    base::CloseProcessHandle(process);
    return;
  }
  debug_context_->SetDebugThread(dbg_thread);
  // System can not reallocate pid until we close process handle. So using
  // pid in different thread is fine.
  dbg_thread->message_loop()->PostTask(FROM_HERE,
      base::Bind(&NaClProcessHost::DebugContext::AttachDebugger,
                 debug_context_, peer_pid, process));
}
#else
void NaClProcessHost::OnChannelConnected(int32 peer_pid) {
}
#endif


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

  const ChildProcessData& data = process_->GetData();
#if defined(OS_WIN)
  // Copy the process handle into the renderer process.
  if (!DuplicateHandle(base::GetCurrentProcessHandle(),
                       data.handle,
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
  nacl_process_handle = data.handle;
#endif

  // Get the pid of the NaCl process
  base::ProcessId nacl_process_id = base::GetProcId(data.handle);

  ChromeViewHostMsg_LaunchNaCl::WriteReplyParams(
      reply_msg_, handles_for_renderer, nacl_process_handle, nacl_process_id);
  chrome_render_message_filter_->Send(reply_msg_);
  chrome_render_message_filter_ = NULL;
  reply_msg_ = NULL;
  internal_->sockets_for_renderer.clear();

  std::vector<nacl::FileDescriptor> handles_for_sel_ldr;
  for (size_t i = 0; i < internal_->sockets_for_sel_ldr.size(); i++) {
    if (!SendHandleToSelLdr(data.handle,
                            internal_->sockets_for_sel_ldr[i], true,
                            &handles_for_sel_ldr)) {
      delete this;
      return;
    }
  }

  // Send over the IRT file handle.  We don't close our own copy!
  if (!SendHandleToSelLdr(data.handle, irt_file, false, &handles_for_sel_ldr)) {
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

  IPC::Message* start_message =
      new NaClProcessMsg_Start(handles_for_sel_ldr, enable_exception_handling_);
#if defined(OS_WIN)
  if (debug_context_ != NULL) {
    debug_context_->SetStartMessage(start_message);
    debug_context_->SendStartMessage();
  } else {
    process_->Send(start_message);
  }
#else
  process_->Send(start_message);
#endif

  internal_->sockets_for_sel_ldr.clear();
}

bool NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED() << "Invalid message with type = " << msg.type();
  return false;
}
