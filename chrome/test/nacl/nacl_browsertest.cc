// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#if defined(OS_POSIX)
#include <unistd.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

#define TELEMETRY 1

#include "base/command_line.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/content_switches.h"

namespace {

#if defined(OS_WIN)
// crbug.com/98721
#  define MAYBE_SysconfNprocessorsOnln DISABLED_SysconfNprocessorsOnln
#else
#  define MAYBE_SysconfNprocessorsOnln SysconfNprocessorsOnln
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, SimpleLoad, {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNonSfiMode, MAYBE_NONSFI(Messaging)) {
  RunLoadTest(FILE_PATH_LITERAL("libc_free.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNonSfiMode, MAYBE_NONSFI(Irt)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_test.html"));
}

NACL_BROWSER_TEST_F(NaClBrowserTest, ExitStatus0, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit0&expected_exit=0"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, ExitStatus254, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit254&expected_exit=254"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, ExitStatusNeg2, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exitneg2&expected_exit=254"));
})

#if defined(ADDRESS_SANITIZER)
#define Maybe_PPAPICore DISABLED_PPAPICore
#else
#define Maybe_PPAPICore PPAPICore
#endif
NACL_BROWSER_TEST_F(NaClBrowserTest, Maybe_PPAPICore, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppb_core.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, PPAPIPPBInstance, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppb_instance.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, PPAPIPPPInstance, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppp_instance.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, ProgressEvents, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_progress_events.html"));
})

// Note: currently not run on PNaCl because crash throttling causes the last few
// tests to fail for the wrong reasons.  Enabling this test would also require
// creating a new set of manifests because shared NaCl/PNaCl manifests are not
// allowed.  Also not run on GLibc because it's a large test that is at risk of
// causing timeouts.
// crbug/338444
#if defined(OS_WIN)
#define MAYBE_Bad DISABLED_Bad
#else
#define MAYBE_Bad Bad
#endif
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, MAYBE_Bad) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_bad.html"));
}

// partially_invalid.c does not have an ARM version of its asm.
#if !defined(__arm__)
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, BadNative) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_bad_native.html"));
}
#endif

#if defined(OS_WIN)
// crbug.com/98721
#  define MAYBE_Crash DISABLED_Crash
#elif defined(OS_LINUX)
// crbug.com/366334
#  define MAYBE_Crash DISABLED_Crash
#else
#  define MAYBE_Crash Crash
#endif
NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_Crash, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_crash.html"));
})

// PNaCl version does not work.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, ManifestFile) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pm_manifest_file_test.html"));
}
IN_PROC_BROWSER_TEST_F(NaClBrowserTestGLibc, MAYBE_GLIBC(ManifestFile)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pm_manifest_file_test.html"));
}
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, PreInitManifestFile) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_pre_init_manifest_file_test.html"));
}
IN_PROC_BROWSER_TEST_F(NaClBrowserTestGLibc,
                       MAYBE_GLIBC(PreInitManifestFile)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_pre_init_manifest_file_test.html"));
}
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, IrtManifestFile) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_manifest_file_test.html"));
}
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclNonSfi,
                       MAYBE_PNACL_NONSFI(IrtManifestFile)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_manifest_file_test.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlib, IrtException) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_exception_test.html"));
}
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclNonSfi,
                       MAYBE_PNACL_NONSFI(IrtException)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("irt_exception_test.html"));
}

NACL_BROWSER_TEST_F(NaClBrowserTest, Nameservice, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pm_nameservice_test.html"));
})

// Some versions of Visual Studio does not like preprocessor
// conditionals inside the argument of a macro, so we put the
// conditionals on a helper function.  We are already in an anonymous
// namespace, so the name of the helper is not visible in external
// scope.
#if defined(OS_POSIX)
base::FilePath::StringType NumberOfCoresAsFilePathString() {
  char string_rep[23];
  long nprocessors = sysconf(_SC_NPROCESSORS_ONLN);
#if TELEMETRY
  fprintf(stderr, "browser says nprocessors = %ld\n", nprocessors);
  fflush(NULL);
#endif
  snprintf(string_rep, sizeof string_rep, "%ld", nprocessors);
  return string_rep;
}
#elif defined(OS_WIN)
base::FilePath::StringType NumberOfCoresAsFilePathString() {
  wchar_t string_rep[23];
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
#if TELEMETRY
  fprintf(stderr, "browser says nprocessors = %d\n",
          system_info.dwNumberOfProcessors);
  fflush(NULL);
#endif
  _snwprintf_s(string_rep, sizeof string_rep / sizeof string_rep[0], _TRUNCATE,
               L"%u", system_info.dwNumberOfProcessors);
  return string_rep;
}
#endif

#if TELEMETRY
static void PathTelemetry(base::FilePath::StringType const &path) {
# if defined(OS_WIN)
    fwprintf(stderr, L"path = %s\n", path.c_str());
# else
    fprintf(stderr, "path = %s\n", path.c_str());
# endif
    fflush(NULL);
}
#else
static void PathTelemetry(base::FilePath::StringType const &path) {
  (void) path;
}
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_SysconfNprocessorsOnln, {
    base::FilePath::StringType path =
      FILE_PATH_LITERAL("sysconf_nprocessors_onln_test.html?cpu_count=");
    path = path + NumberOfCoresAsFilePathString();
    PathTelemetry(path);
    RunNaClIntegrationTest(path);
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, CrossOriginCORS) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/cors.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, CrossOriginFail) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/fail.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, SameOriginCookie) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/same_origin_cookie.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, CORSNoCookie) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/cors_no_cookie.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, RelativeManifest) {
  RunLoadTest(FILE_PATH_LITERAL("manifest/relative_manifest.html"));
}

// Test with the NaCl debug flag turned on.
class NaClBrowserTestPnaclDebug : public NaClBrowserTestPnacl {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    NaClBrowserTestPnacl::SetUpCommandLine(command_line);
    // Turn on debugging to influence the PNaCl URL loaded
    command_line->AppendSwitch(switches::kEnableNaClDebug);
    // On windows, the debug stub requires --no-sandbox:
    // crbug.com/265624
#if defined(OS_WIN)
    command_line->AppendSwitch(switches::kNoSandbox);
#endif
  }

  // On some platforms this test does not work.
  bool TestIsBroken() {
    // TODO(jvoung): Make this test work on Windows 32-bit. When --no-sandbox
    // is used, the required 1GB sandbox address space is not reserved.
    // (see note in chrome/browser/nacl_host/test/nacl_gdb_browsertest.cc)
#if defined(OS_WIN)
    if (base::win::OSInfo::GetInstance()->wow64_status() ==
        base::win::OSInfo::WOW64_DISABLED &&
        base::win::OSInfo::GetInstance()->architecture() ==
        base::win::OSInfo::X86_ARCHITECTURE) {
      return true;
    }
#endif
    return false;
  }

  void StartTestScript(base::ProcessHandle* test_process,
                       int debug_stub_port) {
    // We call a python script that speaks to the debug stub, and
    // lets the app continue, so that the load progress event completes.
    CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python")));
    base::FilePath script;
    PathService::Get(base::DIR_SOURCE_ROOT, &script);
    script = script.AppendASCII(
        "chrome/browser/nacl_host/test/debug_stub_browser_tests.py");
    cmd.AppendArgPath(script);
    cmd.AppendArg(base::IntToString(debug_stub_port));
    cmd.AppendArg("continue");
    LOG(INFO) << cmd.GetCommandLineString();
    base::LaunchProcess(cmd, base::LaunchOptions(), test_process);
  }

  void RunWithTestDebugger(const base::FilePath::StringType& test_url) {
    base::ProcessHandle test_script;
    scoped_ptr<base::Environment> env(base::Environment::Create());
    nacl::NaClBrowser::GetInstance()->SetGdbDebugStubPortListener(
        base::Bind(&NaClBrowserTestPnaclDebug::StartTestScript,
                   base::Unretained(this), &test_script));
    // Turn on debug stub logging.
    env->SetVar("NACLVERBOSITY", "1");
    RunLoadTest(test_url);
    env->UnSetVar("NACLVERBOSITY");
    nacl::NaClBrowser::GetInstance()->ClearGdbDebugStubPortListener();
    int exit_code;
    LOG(INFO) << "Waiting for script to exit (which waits for embed to die).";
    base::WaitForExitCode(test_script, &exit_code);
    EXPECT_EQ(0, exit_code);
  }
};

// Test with the NaCl debug flag turned on, but mask off every URL
// so that nothing is actually debugged.
class NaClBrowserTestPnaclDebugMasked : public NaClBrowserTestPnaclDebug {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    NaClBrowserTestPnaclDebug::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kNaClDebugMask,
                                    "!<all_urls>");
  }
};

// The tests which actually start a debug session must use the debug stub
// to continue the app startup. However, NaCl on windows can't open the
// debug stub socket in the browser process as needed by the test.
// See http://crbug.com/157312.
#if defined(OS_WIN)
#define MAYBE_PnaclDebugURLFlagAndURL DISABLED_PnaclDebugURLFlagAndURL
#define MAYBE_PnaclDebugURLFlagNoURL DISABLED_PnaclDebugURLFlagNoURL
#else
#define MAYBE_PnaclDebugURLFlagAndURL PnaclDebugURLFlagAndURL
#define MAYBE_PnaclDebugURLFlagNoURL PnaclDebugURLFlagNoURL
#endif
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDebug,
                       MAYBE_PnaclDebugURLFlagAndURL) {
  RunWithTestDebugger(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_has_debug.nmf"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDebug,
                       MAYBE_PnaclDebugURLFlagNoURL) {
  RunWithTestDebugger(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_no_debug.nmf"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclDebugURLFlagOff)) {
  RunLoadTest(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_has_debug_flag_off.nmf"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDebugMasked,
                       MAYBE_PNACL(PnaclDebugURLFlagMaskedOff)) {
  if (TestIsBroken()) {
    return;
  }
  // If the mask excludes debugging, it's as if the flag was off.
  RunLoadTest(FILE_PATH_LITERAL(
      "pnacl_debug_url.html?nmf_file=pnacl_has_debug_flag_off.nmf"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclErrorHandling)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pnacl_error_handling.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclNMFOptionsO0)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_0"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclNMFOptionsO2)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_2"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclNMFOptionsOlarge)) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_large"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclDyncodeSyscallDisabled)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pnacl_dyncode_syscall_disabled.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       MAYBE_PNACL(PnaclExceptionHandlingDisabled)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pnacl_exception_handling_disabled.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl, PnaclMimeType) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_mime_type.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclDisabled, PnaclMimeType) {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_mime_type.html"));
}

class NaClBrowserTestNewlibStdoutPM : public NaClBrowserTestNewlib {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Env needs to be set early because nacl_helper is spawned before the test
    // body on Linux.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("NACL_EXE_STDOUT", "DEBUG_ONLY:dev://postmessage");
    NaClBrowserTestNewlib::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStdoutPM, RedirectFg0) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stdout&thread=fg&delay_us=0"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStdoutPM, RedirectBg0) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stdout&thread=bg&delay_us=0"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStdoutPM, RedirectFg1) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stdout&thread=fg&delay_us=1000000"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStdoutPM, RedirectBg1) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stdout&thread=bg&delay_us=1000000"));
}

class NaClBrowserTestNewlibStderrPM : public NaClBrowserTestNewlib {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Env needs to be set early because nacl_helper is spawned before the test
    // body on Linux.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("NACL_EXE_STDERR", "DEBUG_ONLY:dev://postmessage");
    NaClBrowserTestNewlib::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStderrPM, RedirectFg0) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stderr&thread=fg&delay_us=0"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStderrPM, RedirectBg0) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stderr&thread=bg&delay_us=0"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStderrPM, RedirectFg1) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stderr&thread=fg&delay_us=1000000"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibStderrPM, RedirectBg1) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_redir_test.html?stream=stderr&thread=bg&delay_us=1000000"));
}

// TODO(ncbray) support glibc and PNaCl
#if defined(OS_MACOSX)
// crbug.com/375894
#define MAYBE_MimeHandler DISABLED_MimeHandler
#else
#define MAYBE_MimeHandler MimeHandler
#endif
IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibExtension, MAYBE_MimeHandler) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "ppapi_extension_mime_handler.html"));
}

}  // namespace
