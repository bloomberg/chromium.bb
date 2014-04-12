// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/zygote/zygote_main.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/linux_util.h"
#include "base/native_library.h"
#include "base/pickle.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/pepper_plugin_list.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/sandbox_linux.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "content/zygote/zygote_linux.h"
#include "crypto/nss_util.h"
#include "sandbox/linux/services/init_process_reaper.h"
#include "sandbox/linux/services/libc_urandom_override.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

#if defined(OS_LINUX)
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/signal.h>
#else
#include <signal.h>
#endif

#if defined(ENABLE_WEBRTC)
#include "third_party/libjingle/overrides/init_webrtc.h"
#endif

namespace content {

// See http://code.google.com/p/chromium/wiki/LinuxZygote

static void ProxyLocaltimeCallToBrowser(time_t input, struct tm* output,
                                        char* timezone_out,
                                        size_t timezone_out_len) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_LOCALTIME);
  request.WriteString(
      std::string(reinterpret_cast<char*>(&input), sizeof(input)));

  uint8_t reply_buf[512];
  const ssize_t r = UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), reply_buf, sizeof(reply_buf), NULL,
      request);
  if (r == -1) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  PickleIterator iter(reply);
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
static LocaltimeFunction g_libc_localtime64;
static LocaltimeRFunction g_libc_localtime_r;
static LocaltimeRFunction g_libc_localtime64_r;

static void InitLibcLocaltimeFunctions() {
  g_libc_localtime = reinterpret_cast<LocaltimeFunction>(
      dlsym(RTLD_NEXT, "localtime"));
  g_libc_localtime64 = reinterpret_cast<LocaltimeFunction>(
      dlsym(RTLD_NEXT, "localtime64"));
  g_libc_localtime_r = reinterpret_cast<LocaltimeRFunction>(
      dlsym(RTLD_NEXT, "localtime_r"));
  g_libc_localtime64_r = reinterpret_cast<LocaltimeRFunction>(
      dlsym(RTLD_NEXT, "localtime64_r"));

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
  if (!g_libc_localtime64)
    g_libc_localtime64 = g_libc_localtime;
  if (!g_libc_localtime_r)
    g_libc_localtime_r = gmtime_r;
  if (!g_libc_localtime64_r)
    g_libc_localtime64_r = g_libc_localtime_r;
}

#if defined(MEMORY_SANITIZER)
void msan_unpoison_string(const char *s) {
  if (!s) return;
  // Can't call strlen() on an uninitialized string. Instead, unpoison byte by
  // byte until the string is over.
  do {
    __msan_unpoison(s, sizeof(*s));
  } while(*(s++));
}
#endif

// Define localtime_override() function with asm name "localtime", so that all
// references to localtime() will resolve to this function. Notice that we need
// to set visibility attribute to "default" to export the symbol, as it is set
// to "hidden" by default in chrome per build/common.gypi.
__attribute__ ((__visibility__("default")))
struct tm* localtime_override(const time_t* timep) __asm__ ("localtime");

__attribute__ ((__visibility__("default")))
struct tm* localtime_override(const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime(timep);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

// Use same trick to override localtime64(), localtime_r() and localtime64_r().
__attribute__ ((__visibility__("default")))
struct tm* localtime64_override(const time_t* timep) __asm__ ("localtime64");

__attribute__ ((__visibility__("default")))
struct tm* localtime64_override(const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime64(timep);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

__attribute__ ((__visibility__("default")))
struct tm* localtime_r_override(const time_t* timep,
                                struct tm* result) __asm__ ("localtime_r");

__attribute__ ((__visibility__("default")))
struct tm* localtime_r_override(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime_r(timep, result);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

__attribute__ ((__visibility__("default")))
struct tm* localtime64_r_override(const time_t* timep,
                                  struct tm* result) __asm__ ("localtime64_r");

__attribute__ ((__visibility__("default")))
struct tm* localtime64_r_override(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  } else {
    CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                             InitLibcLocaltimeFunctions));
    struct tm* res = g_libc_localtime64_r(timep, result);
#if defined(MEMORY_SANITIZER)
    if (res) __msan_unpoison(res, sizeof(*res));
    if (res->tm_zone) msan_unpoison_string(res->tm_zone);
#endif
    return res;
  }
}

#if defined(ENABLE_PLUGINS)
// Loads the (native) libraries but does not initialize them (i.e., does not
// call PPP_InitializeModule). This is needed by the zygote on Linux to get
// access to the plugins before entering the sandbox.
void PreloadPepperPlugins() {
  std::vector<PepperPluginInfo> plugins;
  ComputePepperPluginList(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!plugins[i].is_internal && plugins[i].is_sandboxed) {
      base::NativeLibraryLoadError error;
      base::NativeLibrary library = base::LoadNativeLibrary(plugins[i].path,
                                                            &error);
      VLOG_IF(1, !library) << "Unable to load plugin "
                           << plugins[i].path.value() << " "
                           << error.ToString();

      (void)library;  // Prevent release-mode warning.
    }
  }
}
#endif

// This function triggers the static and lazy construction of objects that need
// to be created before imposing the sandbox.
static void ZygotePreSandboxInit() {
  base::RandUint64();

  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::MaxSharedMemorySize();
  base::SysInfo::NumberOfProcessors();

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
#if defined(ENABLE_PLUGINS)
  // Ensure access to the Pepper plugins before the sandbox is turned on.
  PreloadPepperPlugins();
#endif
#if defined(ENABLE_WEBRTC)
  InitializeWebRtcModule();
#endif
  SkFontConfigInterface::SetGlobal(
      new FontConfigIPC(GetSandboxFD()))->unref();
}

static void CloseFdAndHandleEintr(int fd) {
  close(fd);
}

static bool CreateInitProcessReaper() {
  // This "magic" socket must only appear in one process, so make sure
  // it gets closed in the parent after fork().
  base::Closure zygoteid_fd_closer =
      base::Bind(CloseFdAndHandleEintr, kZygoteIdFd);
  // The current process becomes init(1), this function returns from a
  // newly created process.
  const bool init_created =
      sandbox::CreateInitProcessReaper(&zygoteid_fd_closer);
  if (!init_created) {
    LOG(ERROR) << "Error creating an init process to reap zombies";
    return false;
  }
  return true;
}

// Enter the setuid sandbox. This requires the current process to have been
// created through the setuid sandbox.
static bool EnterSuidSandbox(sandbox::SetuidSandboxClient* setuid_sandbox) {
  DCHECK(setuid_sandbox);
  DCHECK(setuid_sandbox->IsSuidSandboxChild());

  // Use the SUID sandbox.  This still allows the seccomp sandbox to
  // be enabled by the process later.

  if (!setuid_sandbox->IsSuidSandboxUpToDate()) {
    LOG(WARNING) <<
        "You are using a wrong version of the setuid binary!\n"
        "Please read "
        "https://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment."
        "\n\n";
  }

  if (!setuid_sandbox->ChrootMe())
    return false;

  if (setuid_sandbox->IsInNewPIDNamespace()) {
    CHECK_EQ(1, getpid())
        << "The SUID sandbox created a new PID namespace but Zygote "
           "is not the init process. Please, make sure the SUID "
           "binary is up to date.";
  }

  if (getpid() == 1) {
    // The setuid sandbox has created a new PID namespace and we need
    // to assume the role of init.
    CHECK(CreateInitProcessReaper());
  }

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

  return true;
}

static void EnterLayerOneSandbox(LinuxSandbox* linux_sandbox) {
  DCHECK(linux_sandbox);

  ZygotePreSandboxInit();

  // Check that the pre-sandbox initialization didn't spawn threads.
#if !defined(THREAD_SANITIZER)
  DCHECK(linux_sandbox->IsSingleThreaded());
#endif

  sandbox::SetuidSandboxClient* setuid_sandbox =
      linux_sandbox->setuid_sandbox_client();

  if (setuid_sandbox->IsSuidSandboxChild()) {
    CHECK(EnterSuidSandbox(setuid_sandbox)) << "Failed to enter setuid sandbox";
  }
}

bool ZygoteMain(const MainFunctionParams& params,
                ZygoteForkDelegate* forkdelegate) {
  g_am_zygote_or_renderer = true;
  sandbox::InitLibcUrandomOverrides();

  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  // This will pre-initialize the various sandboxes that need it.
  linux_sandbox->PreinitializeSandbox();

  if (forkdelegate != NULL) {
    VLOG(1) << "ZygoteMain: initializing fork delegate";
    forkdelegate->Init(GetSandboxFD());
  } else {
    VLOG(1) << "ZygoteMain: fork delegate is NULL";
  }

  // Turn on the first layer of the sandbox if the configuration warrants it.
  EnterLayerOneSandbox(linux_sandbox);

  int sandbox_flags = linux_sandbox->GetStatus();

  Zygote zygote(sandbox_flags, forkdelegate);
  // This function call can return multiple times, once per fork().
  return zygote.ProcessRequests();
}

}  // namespace content
