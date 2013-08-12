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

#include "chrome/test/nacl/nacl_browsertest_util.h"

namespace {

// These tests fail on Linux ASAN bots: <http://crbug.com/161709>.
#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_SimpleLoad DISABLED_SimpleLoad
#define MAYBE_ExitStatus0 DISABLED_ExitStatus0
#define MAYBE_ExitStatus254 DISABLED_ExitStatus254
#define MAYBE_ExitStatusNeg2 DISABLED_ExitStatusNeg2
#define MAYBE_PPAPICore DISABLED_PPAPICore
#define MAYBE_ProgressEvents DISABLED_ProgressEvents
#define MAYBE_PnaclMimeType DISABLED_PnaclMimeType
#define MAYBE_CrossOriginCORS DISABLED_CrossOriginCORS
#define MAYBE_CrossOriginFail DISABLED_CrossOriginFail
#define MAYBE_SameOriginCookie DISABLED_SameOriginCookie
#define MAYBE_CORSNoCookie DISABLED_CORSNoCookie
#define MAYBE_SysconfNprocessorsOnln DISABLED_SysconfNprocessorsOnln
#else
#define MAYBE_SimpleLoad SimpleLoad
#define MAYBE_ExitStatus0 ExitStatus0
#define MAYBE_ExitStatus254 ExitStatus254
#define MAYBE_ExitStatusNeg2 ExitStatusNeg2
#define MAYBE_PPAPICore PPAPICore
#define MAYBE_ProgressEvents ProgressEvents
#define MAYBE_PnaclMimeType PnaclMimeType
#define MAYBE_CrossOriginCORS CrossOriginCORS
#define MAYBE_CrossOriginFail CrossOriginFail
#define MAYBE_SameOriginCookie SameOriginCookie
#define MAYBE_CORSNoCookie CORSNoCookie
# if defined(OS_WIN)
#  define MAYBE_SysconfNprocessorsOnln DISABLED_SysconfNprocessorsOnln
# else
#  define MAYBE_SysconfNprocessorsOnln SysconfNprocessorsOnln
# endif
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_SimpleLoad, {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclWithOldCache,
                       MAYBE_PNACL(SimpleLoad)) {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclWithOldCache,
                       MAYBE_PNACL(PnaclErrorHandling)) {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("pnacl_error_handling.html"));
}

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_ExitStatus0, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit0&expected_exit=0"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_ExitStatus254, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit254&expected_exit=254"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_ExitStatusNeg2, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exitneg2&expected_exit=254"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_PPAPICore, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppb_core.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_ProgressEvents, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_progress_events.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_PnaclMimeType, {
  RunLoadTest(FILE_PATH_LITERAL("pnacl_mime_type.html"));
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

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, MAYBE_CrossOriginCORS) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/cors.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, MAYBE_CrossOriginFail) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/fail.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, MAYBE_SameOriginCookie) {
  RunLoadTest(FILE_PATH_LITERAL("cross_origin/same_origin_cookie.html"));
}

IN_PROC_BROWSER_TEST_F(NaClBrowserTestStatic, MAYBE_CORSNoCookie) {
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

}  // namespace
