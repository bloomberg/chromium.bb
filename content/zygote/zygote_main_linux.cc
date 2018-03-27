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

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/protected_memory.h"
#include "base/memory/protected_memory_cfi.h"
#include "base/native_library.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/common/common_sandbox_support_linux.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "content/zygote/zygote_linux.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/init_process_reaper.h"
#include "sandbox/linux/services/libc_interceptor.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "services/service_manager/sandbox/linux/sandbox_debug_handling_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "third_party/WebKit/public/platform/WebFontRenderStyle.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
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

void CloseFds(const std::vector<int>& fds) {
  for (const auto& it : fds) {
    PCHECK(0 == IGNORE_EINTR(close(it)));
  }
}

base::OnceClosure ClosureFromTwoClosures(base::OnceClosure one,
                                         base::OnceClosure two) {
  return base::BindOnce(
      [](base::OnceClosure one, base::OnceClosure two) {
        if (!one.is_null())
          std::move(one).Run();
        if (!two.is_null())
          std::move(two).Run();
      },
      std::move(one), std::move(two));
}

}  // namespace

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

  SkFontConfigInterface::SetGlobal(new FontConfigIPC(GetSandboxFD()))->unref();

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

    blink::WebFontRenderStyle::SetSkiaFontManager(
        SkFontMgr_New_Android(&custom));
  }
}

static bool CreateInitProcessReaper(
    base::OnceClosure post_fork_parent_callback) {
  // The current process becomes init(1), this function returns from a
  // newly created process.
  if (!sandbox::CreateInitProcessReaper(std::move(post_fork_parent_callback))) {
    LOG(ERROR) << "Error creating an init process to reap zombies";
    return false;
  }
  return true;
}

// Enter the setuid sandbox. This requires the current process to have been
// created through the setuid sandbox.
static bool EnterSuidSandbox(sandbox::SetuidSandboxClient* setuid_sandbox,
                             base::OnceClosure post_fork_parent_callback) {
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
    CHECK(CreateInitProcessReaper(std::move(post_fork_parent_callback)));
  }

  CHECK(service_manager::SandboxDebugHandling::SetDumpableStatusAndHandlers());
  return true;
}

static void DropAllCapabilities(int proc_fd) {
  CHECK(sandbox::Credentials::DropAllCapabilities(proc_fd));
}

static void EnterNamespaceSandbox(service_manager::SandboxLinux* linux_sandbox,
                                  base::OnceClosure post_fork_parent_callback) {
  linux_sandbox->EngageNamespaceSandbox(true /* from_zygote */);
  if (getpid() == 1) {
    CHECK(CreateInitProcessReaper(ClosureFromTwoClosures(
        base::BindOnce(DropAllCapabilities, linux_sandbox->proc_fd()),
        std::move(post_fork_parent_callback))));
  }
}

static void EnterLayerOneSandbox(service_manager::SandboxLinux* linux_sandbox,
                                 const bool using_layer1_sandbox,
                                 base::OnceClosure post_fork_parent_callback) {
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
    CHECK(
        EnterSuidSandbox(setuid_sandbox, std::move(post_fork_parent_callback)))
        << "Failed to enter setuid sandbox";
  } else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    EnterNamespaceSandbox(linux_sandbox, std::move(post_fork_parent_callback));
  } else {
    CHECK(!using_layer1_sandbox);
  }
}

bool ZygoteMain(
    std::vector<std::unique_ptr<ZygoteForkDelegate>> fork_delegates) {
  sandbox::SetAmZygoteOrRenderer(true, GetSandboxFD());

  auto* linux_sandbox = service_manager::SandboxLinux::GetInstance();

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

  // Turn on the first layer of the sandbox if the configuration warrants it.
  EnterLayerOneSandbox(
      linux_sandbox, using_layer1_sandbox,
      base::BindOnce(CloseFds, linux_sandbox->GetFileDescriptorsToClose()));

  const int sandbox_flags = linux_sandbox->GetStatus();
  const bool setuid_sandbox_engaged =
      !!(sandbox_flags & service_manager::SandboxLinux::kSUID);
  CHECK_EQ(using_setuid_sandbox, setuid_sandbox_engaged);

  const bool namespace_sandbox_engaged =
      !!(sandbox_flags & service_manager::SandboxLinux::kUserNS);
  CHECK_EQ(using_namespace_sandbox, namespace_sandbox_engaged);

  Zygote zygote(sandbox_flags, std::move(fork_delegates),
                base::GlobalDescriptors::Descriptor(
                    static_cast<uint32_t>(kSandboxIPCChannel), GetSandboxFD()));

  // This function call can return multiple times, once per fork().
  return zygote.ProcessRequests();
}

}  // namespace content
