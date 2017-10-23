// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/zygote/zygote_main.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/native_library.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/sandbox_linux/sandbox_debug_handling_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/common/common_sandbox_support_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_linux.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "content/zygote/zygote_linux.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/init_process_reaper.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/pepper_plugin_info.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "third_party/webrtc_overrides/init_webrtc.h"  // nogncheck
#endif

namespace content {

namespace {

base::LazyInstance<std::set<std::string>>::Leaky g_timezones =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky g_timezones_lock =
    LAZY_INSTANCE_INITIALIZER;

void CloseFds(const std::vector<int>& fds) {
  for (const auto& it : fds) {
    PCHECK(0 == IGNORE_EINTR(close(it)));
  }
}

void RunTwoClosures(const base::Closure* first, const base::Closure* second) {
  first->Run();
  second->Run();
}

bool ReadTimeStruct(base::PickleIterator* iter,
                    struct tm* output,
                    char* timezone_out,
                    size_t timezone_out_len) {
  int result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_sec = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_min = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_hour = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_mday = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_mon = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_year = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_wday = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_yday = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_isdst = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_gmtoff = result;

  std::string timezone;
  if (!iter->ReadString(&timezone))
    return false;
  if (timezone_out_len) {
    const size_t copy_len = std::min(timezone_out_len - 1, timezone.size());
    memcpy(timezone_out, timezone.data(), copy_len);
    timezone_out[copy_len] = 0;
    output->tm_zone = timezone_out;
  } else {
    base::AutoLock lock(g_timezones_lock.Get());
    auto ret_pair = g_timezones.Get().insert(timezone);
    output->tm_zone = ret_pair.first->c_str();
  }

  return true;
}

}  // namespace

// See https://chromium.googlesource.com/chromium/src/+/master/docs/linux_zygote.md

static void ProxyLocaltimeCallToBrowser(time_t input, struct tm* output,
                                        char* timezone_out,
                                        size_t timezone_out_len) {
  base::Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_LOCALTIME);
  request.WriteString(
      std::string(reinterpret_cast<char*>(&input), sizeof(input)));

  memset(output, 0, sizeof(struct tm));

  uint8_t reply_buf[512];
  const ssize_t r = base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), reply_buf, sizeof(reply_buf), NULL, request);
  if (r == -1) {
    return;
  }

  base::Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  base::PickleIterator iter(reply);
  if (!ReadTimeStruct(&iter, output, timezone_out, timezone_out_len)) {
    memset(output, 0, sizeof(struct tm));
  }
}

static bool g_am_zygote_or_renderer = false;
static bool g_use_localtime_override = true;

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
// (https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md)
// or they can use dlsym with RTLD_NEXT to resolve the symbol, ignoring any
// symbols in the current module.
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

// Define localtime_override() function with asm name "localtime", so that all
// references to localtime() will resolve to this function. Notice that we need
// to set visibility attribute to "default" to export the symbol, as it is set
// to "hidden" by default in chrome per build/common.gypi.
__attribute__ ((__visibility__("default")))
struct tm* localtime_override(const time_t* timep) __asm__ ("localtime");

__attribute__ ((__visibility__("default")))
struct tm* localtime_override(const time_t* timep) {
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  }

  CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                           InitLibcLocaltimeFunctions));
  struct tm* res = g_libc_localtime(timep);
#if defined(MEMORY_SANITIZER)
  if (res) __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

// Use same trick to override localtime64(), localtime_r() and localtime64_r().
__attribute__ ((__visibility__("default")))
struct tm* localtime64_override(const time_t* timep) __asm__ ("localtime64");

__attribute__ ((__visibility__("default")))
struct tm* localtime64_override(const time_t* timep) {
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  }

  CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                           InitLibcLocaltimeFunctions));
  struct tm* res = g_libc_localtime64(timep);
#if defined(MEMORY_SANITIZER)
  if (res) __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

__attribute__ ((__visibility__("default")))
struct tm* localtime_r_override(const time_t* timep,
                                struct tm* result) __asm__ ("localtime_r");

__attribute__ ((__visibility__("default")))
struct tm* localtime_r_override(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  }

  CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                           InitLibcLocaltimeFunctions));
  struct tm* res = g_libc_localtime_r(timep, result);
#if defined(MEMORY_SANITIZER)
  if (res) __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

__attribute__ ((__visibility__("default")))
struct tm* localtime64_r_override(const time_t* timep,
                                  struct tm* result) __asm__ ("localtime64_r");

__attribute__ ((__visibility__("default")))
struct tm* localtime64_r_override(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  }

  CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                           InitLibcLocaltimeFunctions));
  struct tm* res = g_libc_localtime64_r(timep, result);
#if defined(MEMORY_SANITIZER)
  if (res) __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone) __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Loads the (native) libraries but does not initialize them (i.e., does not
// call PPP_InitializeModule). This is needed by the zygote on Linux to get
// access to the plugins before entering the sandbox.
void PreloadPepperPlugins() {
  std::vector<PepperPluginInfo> plugins;
  ComputePepperPluginList(&plugins);
  for (const auto& plugin : plugins) {
    if (!plugin.is_internal) {
      base::NativeLibraryLoadError error;
      base::NativeLibrary library = base::LoadNativeLibrary(plugin.path,
                                                            &error);
      VLOG_IF(1, !library) << "Unable to load plugin "
                           << plugin.path.value() << " "
                           << error.ToString();

      ignore_result(library);  // Prevent release-mode warning.
    }
  }
}
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Loads registered library CDMs but does not initialize them. This is needed by
// the zygote on Linux to get access to the CDMs before entering the sandbox.
void PreloadLibraryCdms() {
  std::vector<CdmInfo> cdms;
  GetContentClient()->AddContentDecryptionModules(&cdms, nullptr);
  for (const auto& cdm : cdms) {
    base::NativeLibraryLoadError error;
    base::NativeLibrary library = base::LoadNativeLibrary(cdm.path, &error);
    VLOG_IF(1, !library) << "Unable to load CDM " << cdm.path.value()
                         << " (error: " << error.ToString() << ")";
    ignore_result(library);  // Prevent release-mode warning.
  }
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

// This function triggers the static and lazy construction of objects that need
// to be created before imposing the sandbox.
static void ZygotePreSandboxInit() {
  base::RandUint64();

  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::NumberOfProcessors();

  // ICU DateFormat class (used in base/time_format.cc) needs to get the
  // Olson timezone ID by accessing the zoneinfo files on disk. After
  // TimeZone::createDefault is called once here, the timezone ID is
  // cached and there's no more need to access the file system.
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

#if defined(ARCH_CPU_ARM_FAMILY)
  // On ARM, BoringSSL requires access to /proc/cpuinfo to determine processor
  // features. Query this before entering the sandbox.
  CRYPTO_library_init();
#endif

  // Pass BoringSSL a copy of the /dev/urandom file descriptor so RAND_bytes
  // will work inside the sandbox.
  RAND_set_urandom_fd(base::GetUrandomFD());

#if BUILDFLAG(ENABLE_PLUGINS)
  // Ensure access to the Pepper plugins before the sandbox is turned on.
  PreloadPepperPlugins();
#endif
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Ensure access to the library CDMs before the sandbox is turned on.
  PreloadLibraryCdms();
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
  InitializeWebRtcModule();
#endif

  SkFontConfigInterface::SetGlobal(
      new FontConfigIPC(GetSandboxFD()))->unref();

  // Set the android SkFontMgr for blink. We need to ensure this is done
  // before the sandbox is initialized to allow the font manager to access
  // font configuration files on disk.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAndroidFontsPath)) {
    std::string android_fonts_dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAndroidFontsPath);

    if (android_fonts_dir.size() > 0 && android_fonts_dir.back() != '/')
      android_fonts_dir += '/';

    SkFontMgr_Android_CustomFonts custom;
    custom.fSystemFontUse =
        SkFontMgr_Android_CustomFonts::SystemFontUse::kOnlyCustom;
    custom.fBasePath = android_fonts_dir.c_str();

    std::string font_config;
    std::string fallback_font_config;
    if (android_fonts_dir.find("kitkat") != std::string::npos) {
      font_config = android_fonts_dir + "system_fonts.xml";
      fallback_font_config = android_fonts_dir + "fallback_fonts.xml";
      custom.fFallbackFontsXml = fallback_font_config.c_str();
    } else {
      font_config = android_fonts_dir + "fonts.xml";
      custom.fFallbackFontsXml = nullptr;
    }
    custom.fFontsXml = font_config.c_str();
    custom.fIsolated = true;

    blink::WebFontRendering::SetSkiaFontManager(SkFontMgr_New_Android(&custom));
  }
}

static bool CreateInitProcessReaper(base::Closure* post_fork_parent_callback) {
  // The current process becomes init(1), this function returns from a
  // newly created process.
  const bool init_created =
      sandbox::CreateInitProcessReaper(post_fork_parent_callback);
  if (!init_created) {
    LOG(ERROR) << "Error creating an init process to reap zombies";
    return false;
  }
  return true;
}

// Enter the setuid sandbox. This requires the current process to have been
// created through the setuid sandbox.
static bool EnterSuidSandbox(sandbox::SetuidSandboxClient* setuid_sandbox,
                             base::Closure* post_fork_parent_callback) {
  DCHECK(setuid_sandbox);
  DCHECK(setuid_sandbox->IsSuidSandboxChild());

  // Use the SUID sandbox.  This still allows the seccomp sandbox to
  // be enabled by the process later.

  if (!setuid_sandbox->IsSuidSandboxUpToDate()) {
    LOG(WARNING) <<
        "You are using a wrong version of the setuid binary!\n"
        "Please read "
        "https://chromium.googlesource.com/chromium/src/+/master/docs/linux_suid_sandbox_development.md."
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
    CHECK(CreateInitProcessReaper(post_fork_parent_callback));
  }

  CHECK(SandboxDebugHandling::SetDumpableStatusAndHandlers());
  return true;
}

static void DropAllCapabilities(int proc_fd) {
  CHECK(sandbox::Credentials::DropAllCapabilities(proc_fd));
}

static void EnterNamespaceSandbox(LinuxSandbox* linux_sandbox,
                                  base::Closure* post_fork_parent_callback) {
  linux_sandbox->EngageNamespaceSandbox();

  if (getpid() == 1) {
    base::Closure drop_all_caps_callback =
        base::Bind(&DropAllCapabilities, linux_sandbox->proc_fd());
    base::Closure callback = base::Bind(
        &RunTwoClosures, &drop_all_caps_callback, post_fork_parent_callback);
    CHECK(CreateInitProcessReaper(&callback));
  }
}

static void EnterLayerOneSandbox(LinuxSandbox* linux_sandbox,
                                 const bool using_layer1_sandbox,
                                 base::Closure* post_fork_parent_callback) {
  DCHECK(linux_sandbox);

  ZygotePreSandboxInit();

// Check that the pre-sandbox initialization didn't spawn threads.
// It's not just our code which may do so - some system-installed libraries
// are known to be culprits, e.g. lttng.
#if !defined(THREAD_SANITIZER)
  CHECK(sandbox::ThreadHelpers::IsSingleThreaded());
#endif

  sandbox::SetuidSandboxClient* setuid_sandbox =
      linux_sandbox->setuid_sandbox_client();
  if (setuid_sandbox->IsSuidSandboxChild()) {
    CHECK(EnterSuidSandbox(setuid_sandbox, post_fork_parent_callback))
        << "Failed to enter setuid sandbox";
  } else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    EnterNamespaceSandbox(linux_sandbox, post_fork_parent_callback);
  } else {
    CHECK(!using_layer1_sandbox);
  }
}

bool ZygoteMain(
    const MainFunctionParams& params,
    std::vector<std::unique_ptr<ZygoteForkDelegate>> fork_delegates) {
  g_am_zygote_or_renderer = true;

  std::vector<int> fds_to_close_post_fork;

  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();

  // Skip pre-initializing sandbox under --no-sandbox for crbug.com/444900.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoSandbox)) {
    // This will pre-initialize the various sandboxes that need it.
    linux_sandbox->PreinitializeSandbox();
  }

  const bool using_setuid_sandbox =
      linux_sandbox->setuid_sandbox_client()->IsSuidSandboxChild();
  const bool using_namespace_sandbox =
      sandbox::NamespaceSandbox::InNewUserNamespace();
  const bool using_layer1_sandbox =
      using_setuid_sandbox || using_namespace_sandbox;

  if (using_setuid_sandbox) {
    linux_sandbox->setuid_sandbox_client()->CloseDummyFile();
  }

  if (using_layer1_sandbox) {
    // Let the ZygoteHost know we're booting up.
    if (!base::UnixDomainSocket::SendMsg(
            kZygoteSocketPairFd, kZygoteBootMessage, sizeof(kZygoteBootMessage),
            std::vector<int>())) {
      // This is not a CHECK failure because the browser process could either
      // crash or quickly exit while the zygote is starting. In either case a
      // zygote crash is not useful. http://crbug.com/692227
      PLOG(ERROR) << "Failed sending zygote boot message";
      _exit(1);
    }
  }

  VLOG(1) << "ZygoteMain: initializing " << fork_delegates.size()
          << " fork delegates";
  for (const auto& fork_delegate : fork_delegates) {
    fork_delegate->Init(GetSandboxFD(), using_layer1_sandbox);
  }

  const std::vector<int> sandbox_fds_to_close_post_fork =
      linux_sandbox->GetFileDescriptorsToClose();

  fds_to_close_post_fork.insert(fds_to_close_post_fork.end(),
                                sandbox_fds_to_close_post_fork.begin(),
                                sandbox_fds_to_close_post_fork.end());
  base::Closure post_fork_parent_callback =
      base::Bind(&CloseFds, fds_to_close_post_fork);

  // Turn on the first layer of the sandbox if the configuration warrants it.
  EnterLayerOneSandbox(linux_sandbox, using_layer1_sandbox,
                       &post_fork_parent_callback);

  // Extra children and file descriptors created that the Zygote must have
  // knowledge of.
  std::vector<pid_t> extra_children;
  std::vector<int> extra_fds;

  const int sandbox_flags = linux_sandbox->GetStatus();

  const bool setuid_sandbox_engaged = sandbox_flags & kSandboxLinuxSUID;
  CHECK_EQ(using_setuid_sandbox, setuid_sandbox_engaged);

  const bool namespace_sandbox_engaged = sandbox_flags & kSandboxLinuxUserNS;
  CHECK_EQ(using_namespace_sandbox, namespace_sandbox_engaged);

  Zygote zygote(sandbox_flags, std::move(fork_delegates), extra_children,
                extra_fds);
  // This function call can return multiple times, once per fork().
  return zygote.ProcessRequests();
}

void DisableLocaltimeOverride() {
  g_use_localtime_override = false;
}

}  // namespace content
