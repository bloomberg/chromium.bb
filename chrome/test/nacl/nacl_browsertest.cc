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

#include "base/environment.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"

namespace {

#if defined(OS_WIN)
// crbug.com/98721
#  define MAYBE_Crash DISABLED_Crash
#  define MAYBE_SysconfNprocessorsOnln DISABLED_SysconfNprocessorsOnln
#else
#  define MAYBE_Crash Crash
#  define MAYBE_SysconfNprocessorsOnln SysconfNprocessorsOnln
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, SimpleLoad, {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
})

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

NACL_BROWSER_TEST_F(NaClBrowserTest, PPAPICore, {
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

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_Crash, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_crash.html"));
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

}  // namespace
