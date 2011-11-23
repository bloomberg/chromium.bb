// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/zygote_host_linux.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/global_descriptors_posix.h"
#include "base/hash_tables.h"
#include "base/linux_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "content/common/chrome_descriptors.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/common/sandbox_methods_linux.h"
#include "content/common/seccomp_sandbox.h"
#include "content/common/set_process_title.h"
#include "content/common/unix_domain_socket_posix.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "skia/ext/SkFontHost_fontconfig_control.h"
#include "unicode/timezone.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"

#if defined(OS_LINUX)
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/signal.h>
#else
#include <signal.h>
#endif

#if defined(CHROMIUM_SELINUX)
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

// http://code.google.com/p/chromium/wiki/LinuxZygote

static const int kBrowserDescriptor = 3;
static const int kMagicSandboxIPCDescriptor = 5;
static const int kZygoteIdDescriptor = 7;
static bool g_suid_sandbox_active = false;

#if defined(SECCOMP_SANDBOX)
static int g_proc_fd = -1;
#endif

#if defined(CHROMIUM_SELINUX)
static void SELinuxTransitionToTypeOrDie(const char* type) {
  security_context_t security_context;
  if (getcon(&security_context))
    LOG(FATAL) << "Cannot get SELinux context";

  context_t context = context_new(security_context);
  context_type_set(context, type);
  const int r = setcon(context_str(context));
  context_free(context);
  freecon(security_context);

  if (r) {
    LOG(FATAL) << "dynamic transition to type '" << type << "' failed. "
                  "(this binary has been built with SELinux support, but maybe "
                  "the policies haven't been loaded into the kernel?)";
  }
}
#endif  // CHROMIUM_SELINUX

// This is the object which implements the zygote. The ZygoteMain function,
// which is called from ChromeMain, simply constructs one of these objects and
// runs it.
class Zygote {
 public:
  Zygote(int sandbox_flags, content::ZygoteForkDelegate* helper)
      : sandbox_flags_(sandbox_flags), helper_(helper) {
    if (helper_)
      helper_->InitialUMA(&initial_uma_name_,
                          &initial_uma_sample_,
                          &initial_uma_boundary_value_);
  }

  bool ProcessRequests() {
    // A SOCK_SEQPACKET socket is installed in fd 3. We get commands from the
    // browser on it.
    // A SOCK_DGRAM is installed in fd 5. This is the sandbox IPC channel.
    // See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

    // We need to accept SIGCHLD, even though our handler is a no-op because
    // otherwise we cannot wait on children. (According to POSIX 2001.)
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIGCHLDHandler;
    CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

    if (g_suid_sandbox_active) {
      // Let the ZygoteHost know we are ready to go.
      // The receiving code is in content/browser/zygote_host_linux.cc.
      std::vector<int> empty;
      bool r = UnixDomainSocket::SendMsg(kBrowserDescriptor, kZygoteMagic,
                                         sizeof(kZygoteMagic), empty);
#if defined(OS_CHROMEOS)
      LOG_IF(WARNING, r) << "Sending zygote magic failed";
      // Exit normally on chromeos because session manager may send SIGTERM
      // right after the process starts and it may fail to send zygote magic
      // number to browser process.
      if (!r)
        _exit(content::RESULT_CODE_NORMAL_EXIT);
#else
      CHECK(r) << "Sending zygote magic failed";
#endif
    }

    for (;;) {
      // This function call can return multiple times, once per fork().
      if (HandleRequestFromBrowser(kBrowserDescriptor))
        return true;
    }
  }

 private:
  // See comment below, where sigaction is called.
  static void SIGCHLDHandler(int signal) { }

  // ---------------------------------------------------------------------------
  // Requests from the browser...

  // Read and process a request from the browser. Returns true if we are in a
  // new process and thus need to unwind back into ChromeMain.
  bool HandleRequestFromBrowser(int fd) {
    std::vector<int> fds;
    static const unsigned kMaxMessageLength = 2048;
    char buf[kMaxMessageLength];
    const ssize_t len = UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);

    if (len == 0 || (len == -1 && errno == ECONNRESET)) {
      // EOF from the browser. We should die.
      _exit(0);
      return false;
    }

    if (len == -1) {
      PLOG(ERROR) << "Error reading message from browser";
      return false;
    }

    Pickle pickle(buf, len);
    void* iter = NULL;

    int kind;
    if (pickle.ReadInt(&iter, &kind)) {
      switch (kind) {
        case ZygoteHost::kCmdFork:
          // This function call can return multiple times, once per fork().
          return HandleForkRequest(fd, pickle, iter, fds);

        case ZygoteHost::kCmdReap:
          if (!fds.empty())
            break;
          HandleReapRequest(fd, pickle, iter);
          return false;
        case ZygoteHost::kCmdGetTerminationStatus:
          if (!fds.empty())
            break;
          HandleGetTerminationStatus(fd, pickle, iter);
          return false;
        case ZygoteHost::kCmdGetSandboxStatus:
          HandleGetSandboxStatus(fd, pickle, iter);
          return false;
        default:
          NOTREACHED();
          break;
      }
    }

    LOG(WARNING) << "Error parsing message from browser";
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);
    return false;
  }

  void HandleReapRequest(int fd, const Pickle& pickle, void* iter) {
    base::ProcessId child;
    base::ProcessId actual_child;

    if (!pickle.ReadInt(&iter, &child)) {
      LOG(WARNING) << "Error parsing reap request from browser";
      return;
    }

    if (g_suid_sandbox_active) {
      actual_child = real_pids_to_sandbox_pids[child];
      if (!actual_child)
        return;
      real_pids_to_sandbox_pids.erase(child);
    } else {
      actual_child = child;
    }

    base::EnsureProcessTerminated(actual_child);
  }

  void HandleGetTerminationStatus(int fd, const Pickle& pickle, void* iter) {
    base::ProcessHandle child;

    if (!pickle.ReadInt(&iter, &child)) {
      LOG(WARNING) << "Error parsing GetTerminationStatus request "
                   << "from browser";
      return;
    }

    base::TerminationStatus status;
    int exit_code;
    if (g_suid_sandbox_active)
      child = real_pids_to_sandbox_pids[child];
    if (child) {
      status = base::GetTerminationStatus(child, &exit_code);
    } else {
      // Assume that if we can't find the child in the sandbox, then
      // it terminated normally.
      status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
      exit_code = content::RESULT_CODE_NORMAL_EXIT;
    }

    Pickle write_pickle;
    write_pickle.WriteInt(static_cast<int>(status));
    write_pickle.WriteInt(exit_code);
    ssize_t written =
        HANDLE_EINTR(write(fd, write_pickle.data(), write_pickle.size()));
    if (written != static_cast<ssize_t>(write_pickle.size()))
      PLOG(ERROR) << "write";
  }

  // This is equivalent to fork(), except that, when using the SUID
  // sandbox, it returns the real PID of the child process as it
  // appears outside the sandbox, rather than returning the PID inside
  // the sandbox. Optionally, it fills in uma_name et al with a report
  // the helper wants to make via UMA_HISTOGRAM_ENUMERATION.
  int ForkWithRealPid(const std::string& process_type, std::vector<int>& fds,
                      const std::string& channel_switch,
                      std::string* uma_name,
                      int* uma_sample, int* uma_boundary_value) {
    const bool use_helper = (helper_ && helper_->CanHelp(process_type,
                                                         uma_name,
                                                         uma_sample,
                                                         uma_boundary_value));
    if (!(use_helper || g_suid_sandbox_active)) {
      return fork();
    }

    int dummy_fd;
    ino_t dummy_inode;
    int pipe_fds[2] = { -1, -1 };
    base::ProcessId pid = 0;

    dummy_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (dummy_fd < 0) {
      LOG(ERROR) << "Failed to create dummy FD";
      goto error;
    }
    if (!base::FileDescriptorGetInode(&dummy_inode, dummy_fd)) {
      LOG(ERROR) << "Failed to get inode for dummy FD";
      goto error;
    }
    if (pipe(pipe_fds) != 0) {
      LOG(ERROR) << "Failed to create pipe";
      goto error;
    }

    if (use_helper) {
      fds.push_back(dummy_fd);
      fds.push_back(pipe_fds[0]);
      pid = helper_->Fork(fds);
    } else {
      pid = fork();
    }
    if (pid < 0) {
      goto error;
    } else if (pid == 0) {
      // In the child process.
      close(pipe_fds[1]);
      base::ProcessId real_pid;
      // Wait until the parent process has discovered our PID.  We
      // should not fork any child processes (which the seccomp
      // sandbox does) until then, because that can interfere with the
      // parent's discovery of our PID.
      if (!file_util::ReadFromFD(pipe_fds[0],
                                 reinterpret_cast<char*>(&real_pid),
                                 sizeof(real_pid))) {
        LOG(FATAL) << "Failed to synchronise with parent zygote process";
      }
      if (real_pid <= 0) {
        LOG(FATAL) << "Invalid pid from parent zygote";
      }
#if defined(OS_LINUX)
      // Sandboxed processes need to send the global, non-namespaced PID when
      // setting up an IPC channel to their parent.
      IPC::Channel::SetGlobalPid(real_pid);
#endif
      close(pipe_fds[0]);
      close(dummy_fd);
      return 0;
    } else {
      // In the parent process.
      close(dummy_fd);
      dummy_fd = -1;
      close(pipe_fds[0]);
      pipe_fds[0] = -1;
      base::ProcessId real_pid;
      if (g_suid_sandbox_active) {
        uint8_t reply_buf[512];
        Pickle request;
        request.WriteInt(LinuxSandbox::METHOD_GET_CHILD_WITH_INODE);
        request.WriteUInt64(dummy_inode);

        const ssize_t r = UnixDomainSocket::SendRecvMsg(
            kMagicSandboxIPCDescriptor, reply_buf, sizeof(reply_buf), NULL,
            request);
        if (r == -1) {
          LOG(ERROR) << "Failed to get child process's real PID";
          goto error;
        }

        Pickle reply(reinterpret_cast<char*>(reply_buf), r);
        void* iter = NULL;
        if (!reply.ReadInt(&iter, &real_pid))
          goto error;
        if (real_pid <= 0) {
          // METHOD_GET_CHILD_WITH_INODE failed. Did the child die already?
          LOG(ERROR) << "METHOD_GET_CHILD_WITH_INODE failed";
          goto error;
        }
        real_pids_to_sandbox_pids[real_pid] = pid;
      }
      if (use_helper) {
        real_pid = pid;
        if (!helper_->AckChild(pipe_fds[1], channel_switch)) {
          LOG(ERROR) << "Failed to synchronise with zygote fork helper";
          goto error;
        }
      } else {
        int written =
            HANDLE_EINTR(write(pipe_fds[1], &real_pid, sizeof(real_pid)));
        if (written != sizeof(real_pid)) {
          LOG(ERROR) << "Failed to synchronise with child process";
          goto error;
        }
      }
      close(pipe_fds[1]);
      return real_pid;
    }

   error:
    if (pid > 0) {
      if (waitpid(pid, NULL, WNOHANG) == -1)
        LOG(ERROR) << "Failed to wait for process";
    }
    if (dummy_fd >= 0)
      close(dummy_fd);
    if (pipe_fds[0] >= 0)
      close(pipe_fds[0]);
    if (pipe_fds[1] >= 0)
      close(pipe_fds[1]);
    return -1;
  }

  // Unpacks process type and arguments from |pickle| and forks a new process.
  // Returns -1 on error, otherwise returns twice, returning 0 to the child
  // process and the child process ID to the parent process, like fork().
  base::ProcessId ReadArgsAndFork(const Pickle& pickle,
                                  void* iter,
                                  std::vector<int>& fds,
                                  std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value) {
    std::vector<std::string> args;
    int argc = 0;
    int numfds = 0;
    base::GlobalDescriptors::Mapping mapping;
    std::string process_type;
    std::string channel_id;
    const std::string channel_id_prefix = std::string("--")
        + switches::kProcessChannelID + std::string("=");

    if (!pickle.ReadString(&iter, &process_type))
      return -1;
    if (!pickle.ReadInt(&iter, &argc))
      return -1;

    for (int i = 0; i < argc; ++i) {
      std::string arg;
      if (!pickle.ReadString(&iter, &arg))
        return -1;
      args.push_back(arg);
      if (arg.compare(0, channel_id_prefix.length(), channel_id_prefix) == 0)
        channel_id = arg;
    }

    if (!pickle.ReadInt(&iter, &numfds))
      return -1;
    if (numfds != static_cast<int>(fds.size()))
      return -1;

    for (int i = 0; i < numfds; ++i) {
      base::GlobalDescriptors::Key key;
      if (!pickle.ReadUInt32(&iter, &key))
        return -1;
      mapping.push_back(std::make_pair(key, fds[i]));
    }

    mapping.push_back(std::make_pair(
        static_cast<uint32_t>(kSandboxIPCChannel), kMagicSandboxIPCDescriptor));

    // Returns twice, once per process.
    base::ProcessId child_pid = ForkWithRealPid(process_type, fds, channel_id,
                                                uma_name, uma_sample,
                                                uma_boundary_value);
    if (!child_pid) {
      // This is the child process.
#if defined(SECCOMP_SANDBOX)
      if (SeccompSandboxEnabled() && g_proc_fd >= 0) {
        // Try to open /proc/self/maps as the seccomp sandbox needs access to it
        int proc_self_maps = openat(g_proc_fd, "self/maps", O_RDONLY);
        if (proc_self_maps >= 0) {
          SeccompSandboxSetProcSelfMaps(proc_self_maps);
        } else {
          PLOG(ERROR) << "openat(/proc/self/maps)";
        }
        close(g_proc_fd);
        g_proc_fd = -1;
      }
#endif

      close(kBrowserDescriptor);  // our socket from the browser
      if (g_suid_sandbox_active)
        close(kZygoteIdDescriptor);  // another socket from the browser
      base::GlobalDescriptors::GetInstance()->Reset(mapping);

#if defined(CHROMIUM_SELINUX)
      SELinuxTransitionToTypeOrDie("chromium_renderer_t");
#endif

      // Reset the process-wide command line to our new command line.
      CommandLine::Reset();
      CommandLine::Init(0, NULL);
      CommandLine::ForCurrentProcess()->InitFromArgv(args);

      // Update the process title. The argv was already cached by the call to
      // SetProcessTitleFromCommandLine in ChromeMain, so we can pass NULL here
      // (we don't have the original argv at this point).
      SetProcessTitleFromCommandLine(NULL);
    } else if (child_pid < 0) {
      LOG(ERROR) << "Zygote could not fork: process_type " << process_type
          << " numfds " << numfds << " child_pid " << child_pid;
    }
    return child_pid;
  }

  // Handle a 'fork' request from the browser: this means that the browser
  // wishes to start a new renderer.  Returns true if we are in a new process,
  // otherwise writes the child_pid back to the browser via |fd|.  Writes a
  // child_pid of -1 on error.
  bool HandleForkRequest(int fd, const Pickle& pickle,
                         void* iter, std::vector<int>& fds) {
    std::string uma_name;
    int uma_sample;
    int uma_boundary_value;
    base::ProcessId child_pid = ReadArgsAndFork(pickle, iter, fds,
                                                &uma_name, &uma_sample,
                                                &uma_boundary_value);
    if (child_pid == 0)
      return true;
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);
    if (uma_name.empty()) {
      // There is no UMA report from this particular fork.
      // Use the initial UMA report if any, and clear that record for next time.
      // Note the swap method here is the efficient way to do this, since
      // we know uma_name is empty.
      uma_name.swap(initial_uma_name_);
      uma_sample = initial_uma_sample_;
      uma_boundary_value = initial_uma_boundary_value_;
    }
    // Must always send reply, as ZygoteHost blocks while waiting for it.
    Pickle reply_pickle;
    reply_pickle.WriteInt(child_pid);
    reply_pickle.WriteString(uma_name);
    if (!uma_name.empty()) {
      reply_pickle.WriteInt(uma_sample);
      reply_pickle.WriteInt(uma_boundary_value);
    }
    if (HANDLE_EINTR(write(fd, reply_pickle.data(), reply_pickle.size())) !=
        static_cast<ssize_t> (reply_pickle.size()))
      PLOG(ERROR) << "write";
    return false;
  }

  bool HandleGetSandboxStatus(int fd, const Pickle& pickle, void* iter) {
    if (HANDLE_EINTR(write(fd, &sandbox_flags_, sizeof(sandbox_flags_)) !=
                     sizeof(sandbox_flags_))) {
      PLOG(ERROR) << "write";
    }

    return false;
  }

  // In the SUID sandbox, we try to use a new PID namespace. Thus the PIDs
  // fork() returns are not the real PIDs, so we need to map the Real PIDS
  // into the sandbox PID namespace.
  typedef base::hash_map<base::ProcessHandle, base::ProcessHandle> ProcessMap;
  ProcessMap real_pids_to_sandbox_pids;

  const int sandbox_flags_;
  content::ZygoteForkDelegate* helper_;

  // These might be set by helper_->InitialUMA.  They supply a UMA
  // enumeration sample we should report on the first fork.
  std::string initial_uma_name_;
  int initial_uma_sample_;
  int initial_uma_boundary_value_;
};

// With SELinux we can carve out a precise sandbox, so we don't have to play
// with intercepting libc calls.
#if !defined(CHROMIUM_SELINUX)

static void ProxyLocaltimeCallToBrowser(time_t input, struct tm* output,
                                        char* timezone_out,
                                        size_t timezone_out_len) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_LOCALTIME);
  request.WriteString(
      std::string(reinterpret_cast<char*>(&input), sizeof(input)));

  uint8_t reply_buf[512];
  const ssize_t r = UnixDomainSocket::SendRecvMsg(
      kMagicSandboxIPCDescriptor, reply_buf, sizeof(reply_buf), NULL, request);
  if (r == -1) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  void* iter = NULL;
  std::string result, timezone;
  if (!reply.ReadString(&iter, &result) ||
      !reply.ReadString(&iter, &timezone) ||
      result.size() != sizeof(struct tm)) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  memcpy(output, result.data(), sizeof(struct tm));
  if (timezone_out_len) {
    const size_t copy_len = std::min(timezone_out_len - 1, timezone.size());
    memcpy(timezone_out, timezone.data(), copy_len);
    timezone_out[copy_len] = 0;
    output->tm_zone = timezone_out;
  } else {
    output->tm_zone = NULL;
  }
}

static bool g_am_zygote_or_renderer = false;

// Sandbox interception of libc calls.
//
// Because we are running in a sandbox certain libc calls will fail (localtime
// being the motivating example - it needs to read /etc/localtime). We need to
// intercept these calls and proxy them to the browser. However, these calls
// may come from us or from our libraries. In some cases we can't just change
// our code.
//
// It's for these cases that we have the following setup:
//
// We define global functions for those functions which we wish to override.
// Since we will be first in the dynamic resolution order, the dynamic linker
// will point callers to our versions of these functions. However, we have the
// same binary for both the browser and the renderers, which means that our
// overrides will apply in the browser too.
//
// The global |g_am_zygote_or_renderer| is true iff we are in a zygote or
// renderer process. It's set in ZygoteMain and inherited by the renderers when
// they fork. (This means that it'll be incorrect for global constructor
// functions and before ZygoteMain is called - beware).
//
// Our replacement functions can check this global and either proxy
// the call to the browser over the sandbox IPC
// (http://code.google.com/p/chromium/wiki/LinuxSandboxIPC) or they can use
// dlsym with RTLD_NEXT to resolve the symbol, ignoring any symbols in the
// current module.
//
// Other avenues:
//
// Our first attempt involved some assembly to patch the GOT of the current
// module. This worked, but was platform specific and doesn't catch the case
// where a library makes a call rather than current module.
//
// We also considered patching the function in place, but this would again by
// platform specific and the above technique seems to work well enough.

typedef struct tm* (*LocaltimeFunction)(const time_t* timep);
typedef struct tm* (*LocaltimeRFunction)(const time_t* timep,
                                         struct tm* result);

static pthread_once_t g_libc_localtime_funcs_guard = PTHREAD_ONCE_INIT;
static LocaltimeFunction g_libc_localtime;
static LocaltimeRFunction g_libc_localtime_r;

static void InitLibcLocaltimeFunctions() {
  g_libc_localtime = reinterpret_cast<LocaltimeFunction>(
                         dlsym(RTLD_NEXT, "localtime"));
  g_libc_localtime_r = reinterpret_cast<LocaltimeRFunction>(
                         dlsym(RTLD_NEXT, "localtime_r"));

  if (!g_libc_localtime || !g_libc_localtime_r) {
    // http://code.google.com/p/chromium/issues/detail?id=16800
    //
    // Nvidia's libGL.so overrides dlsym for an unknown reason and replaces
    // it with a version which doesn't work. In this case we'll get a NULL
    // result. There's not a lot we can do at this point, so we just bodge it!
    LOG(ERROR) << "Your system is broken: dlsym doesn't work! This has been "
                  "reported to be caused by Nvidia's libGL. You should expect"
                  " time related functions to misbehave. "
                  "http://code.google.com/p/chromium/issues/detail?id=16800";
  }

  if (!g_libc_localtime)
    g_libc_localtime = gmtime;
  if (!g_libc_localtime_r)
    g_libc_localtime_r = gmtime_r;
}

struct tm* localtime(const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    return g_libc_localtime(timep);
  }
}

struct tm* localtime_r(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    return g_libc_localtime_r(timep, result);
  }
}

#endif  // !CHROMIUM_SELINUX

// This function triggers the static and lazy construction of objects that need
// to be created before imposing the sandbox.
static void PreSandboxInit() {
  base::RandUint64();

  base::SysInfo::MaxSharedMemorySize();

  // ICU DateFormat class (used in base/time_format.cc) needs to get the
  // Olson timezone ID by accessing the zoneinfo files on disk. After
  // TimeZone::createDefault is called once here, the timezone ID is
  // cached and there's no more need to access the file system.
  scoped_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

#if defined(USE_NSS)
  // NSS libraries are loaded before sandbox is activated. This is to allow
  // successful initialization of NSS which tries to load extra library files.
  crypto::LoadNSSLibraries();
#elif defined(USE_OPENSSL)
  // OpenSSL is intentionally not supported in the sandboxed processes, see
  // http://crbug.com/99163. If that ever changes we'll likely need to init
  // OpenSSL here (at least, load the library and error strings).
#else
  // It's possible that another hypothetical crypto stack would not require
  // pre-sandbox init, but more likely this is just a build configuration error.
  #error Which SSL library are you using?
#endif

  // Ensure access to the Pepper plugins before the sandbox is turned on.
  PepperPluginRegistry::PreloadModules();
}

#if !defined(CHROMIUM_SELINUX)
static bool EnterSandbox() {
  // The SUID sandbox sets this environment variable to a file descriptor
  // over which we can signal that we have completed our startup and can be
  // chrooted.
  const char* const sandbox_fd_string = getenv("SBX_D");

  if (sandbox_fd_string) {
    // Use the SUID sandbox.  This still allows the seccomp sandbox to
    // be enabled by the process later.
    g_suid_sandbox_active = true;

    char* endptr;
    const long fd_long = strtol(sandbox_fd_string, &endptr, 10);
    if (!*sandbox_fd_string || *endptr || fd_long < 0 || fd_long > INT_MAX)
      return false;
    const int fd = fd_long;

    PreSandboxInit();

    static const char kMsgChrootMe = 'C';
    static const char kMsgChrootSuccessful = 'O';

    if (HANDLE_EINTR(write(fd, &kMsgChrootMe, 1)) != 1) {
      LOG(ERROR) << "Failed to write to chroot pipe: " << errno;
      return false;
    }

    // We need to reap the chroot helper process in any event:
    wait(NULL);

    char reply;
    if (HANDLE_EINTR(read(fd, &reply, 1)) != 1) {
      LOG(ERROR) << "Failed to read from chroot pipe: " << errno;
      return false;
    }

    if (reply != kMsgChrootSuccessful) {
      LOG(ERROR) << "Error code reply from chroot helper";
      return false;
    }

    SkiaFontConfigSetImplementation(
        new FontConfigIPC(kMagicSandboxIPCDescriptor));

#if !defined(OS_OPENBSD)
    // Previously, we required that the binary be non-readable. This causes the
    // kernel to mark the process as non-dumpable at startup. The thinking was
    // that, although we were putting the renderers into a PID namespace (with
    // the SUID sandbox), they would nonetheless be in the /same/ PID
    // namespace. So they could ptrace each other unless they were non-dumpable.
    //
    // If the binary was readable, then there would be a window between process
    // startup and the point where we set the non-dumpable flag in which a
    // compromised renderer could ptrace attach.
    //
    // However, now that we have a zygote model, only the (trusted) zygote
    // exists at this point and we can set the non-dumpable flag which is
    // inherited by all our renderer children.
    //
    // Note: a non-dumpable process can't be debugged. To debug sandbox-related
    // issues, one can specify --allow-sandbox-debugging to let the process be
    // dumpable.
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kAllowSandboxDebugging)) {
      prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
      if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)) {
        LOG(ERROR) << "Failed to set non-dumpable flag";
        return false;
      }
    }
#endif
#if defined(SECCOMP_SANDBOX)
  } else if (SeccompSandboxEnabled()) {
    PreSandboxInit();
    SkiaFontConfigSetImplementation(
        new FontConfigIPC(kMagicSandboxIPCDescriptor));
#endif
  } else {
    SkiaFontConfigUseDirectImplementation();
  }

  return true;
}
#else  // CHROMIUM_SELINUX

static bool EnterSandbox() {
  PreSandboxInit();
  SkiaFontConfigUseIPCImplementation(kMagicSandboxIPCDescriptor);
  return true;
}

#endif  // CHROMIUM_SELINUX

bool ZygoteMain(const content::MainFunctionParams& params,
                content::ZygoteForkDelegate* forkdelegate) {
#if !defined(CHROMIUM_SELINUX)
  g_am_zygote_or_renderer = true;
#endif

#if defined(SECCOMP_SANDBOX)
  if (SeccompSandboxEnabled()) {
    // The seccomp sandbox needs access to files in /proc, which might be denied
    // after one of the other sandboxes have been started. So, obtain a suitable
    // file handle in advance.
    g_proc_fd = open("/proc", O_DIRECTORY | O_RDONLY);
    if (g_proc_fd < 0) {
      LOG(ERROR) << "WARNING! Cannot access \"/proc\". Disabling seccomp "
          "sandboxing.";
    }
  }
#endif  // SECCOMP_SANDBOX

  if (forkdelegate != NULL) {
    VLOG(1) << "ZygoteMain: initializing fork delegate";
    forkdelegate->Init(getenv("SBX_D") != NULL, // g_suid_sandbox_active,
                       kBrowserDescriptor,
                       kMagicSandboxIPCDescriptor);
  } else {
    VLOG(1) << "ZygoteMain: fork delegate is NULL";
  }

  // Turn on the SELinux or SUID sandbox
  if (!EnterSandbox()) {
    LOG(FATAL) << "Failed to enter sandbox. Fail safe abort. (errno: "
               << errno << ")";
    return false;
  }

  int sandbox_flags = 0;
  if (getenv("SBX_D"))
    sandbox_flags |= ZygoteHost::kSandboxSUID;
  if (getenv("SBX_PID_NS"))
    sandbox_flags |= ZygoteHost::kSandboxPIDNS;
  if (getenv("SBX_NET_NS"))
    sandbox_flags |= ZygoteHost::kSandboxNetNS;

#if defined(SECCOMP_SANDBOX)
  // The seccomp sandbox will be turned on when the renderers start. But we can
  // already check if sufficient support is available so that we only need to
  // print one error message for the entire browser session.
  if (g_proc_fd >= 0 && SeccompSandboxEnabled()) {
    if (!SupportsSeccompSandbox(g_proc_fd)) {
      // There are a good number of users who cannot use the seccomp sandbox
      // (e.g. because their distribution does not enable seccomp mode by
      // default). While we would prefer to deny execution in this case, it
      // seems more realistic to continue in degraded mode.
      LOG(ERROR) << "WARNING! This machine lacks support needed for the "
                    "Seccomp sandbox. Running renderers with Seccomp "
                    "sandboxing disabled.";
    } else {
      VLOG(1) << "Enabling experimental Seccomp sandbox.";
      sandbox_flags |= ZygoteHost::kSandboxSeccomp;
    }
  }
#endif  // SECCOMP_SANDBOX

  Zygote zygote(sandbox_flags, forkdelegate);
  // This function call can return multiple times, once per fork().
  return zygote.ProcessRequests();
}
