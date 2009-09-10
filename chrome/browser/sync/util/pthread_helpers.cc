// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/pthread_helpers.h"

#if (defined(OS_LINUX) || defined(OS_MACOSX))
#include <sys/time.h>
#endif   // (defined(OS_LINUX) || defined(OS_MACOSX))

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/protocol/service_constants.h"

#ifdef OS_WINDOWS

namespace {

// Ensure that we don't bug the user more than once about the process being
// terminated.
base::subtle::AtomicWord g_process_terminating = 0;

struct ThreadStartParams {
  void *(*start) (void* payload);
  void* param;
};

void* ThreadMainProc(void* parameter) {
  ThreadStartParams* tsp = reinterpret_cast<ThreadStartParams*>(parameter);
  void *(*start) (void *) = tsp->start;
  void* param = tsp->param;

  delete tsp;

  void* result = NULL;
  __try {
    result = start(param);
  } __except(EXCEPTION_CONTINUE_SEARCH) {
    // Make sure only one thread complains and exits the process. Other
    // faulting threads simply return.
    if (0 == base::subtle::NoBarrier_CompareAndSwap(
                 &g_process_terminating, 0, 1)) {
      // Service notification means we don't have a recursive event loop inside
      // this call, and so won't suffer recursive exceptions.
      ::MessageBox(NULL,
                   PRODUCT_NAME_STRING
                   L" has suffered a non-recoverable\n"
                   L"exception, and must exit immediately",
                   L"Nonrecoverable Exception",
                   MB_OK | MB_APPLMODAL | MB_SERVICE_NOTIFICATION);

      ::ExitProcess(GetExceptionCode());
    }
  }

  return result;
}

}  // namespace

#endif

thread_handle CreatePThread(void *(*start) (void *), void* parameter) {
#ifdef OS_WINDOWS
  scoped_ptr<ThreadStartParams> param(new ThreadStartParams);
  if (NULL == param.get())
    return NULL;

  param->start = start;
  param->param = parameter;

  pthread_t pthread;
  if (0 != pthread_create(&pthread, NULL, ThreadMainProc, param.get()))
    return NULL;

  // ownership has passed to the new thread
  param.release();

  const HANDLE thread_handle = pthread_getw32threadhandle_np(pthread);
  HANDLE thread_copy;
  // Have to duplicate the thread handle, because when we call
  // pthread_detach(), the handle will get closed as soon as the thread exits.
  // We want to keep the handle indefinitely.
  CHECK(DuplicateHandle(GetCurrentProcess(), thread_handle,
                        GetCurrentProcess(), &thread_copy, 0, FALSE,
                        DUPLICATE_SAME_ACCESS)) <<
      "DuplicateHandle() failed with error " << GetLastError();
  pthread_detach(pthread);
  return thread_copy;
#else
  pthread_t handle;

  int result = pthread_create(&handle, NULL, start, parameter);
  if (result == 0) {
    return handle;
  } else {
    return 0;
  }
#endif
}

struct timespec GetPThreadAbsoluteTime(uint32 ms) {
#ifdef OS_WINDOWS
  FILETIME filenow;
  GetSystemTimeAsFileTime(&filenow);
  ULARGE_INTEGER n;
  n.LowPart = filenow.dwLowDateTime;
  n.HighPart = filenow.dwHighDateTime;
  // Filetime unit is 100-nanosecond intervals
  const int64 ms_ftime = 10000;
  n.QuadPart += ms_ftime * ms;

  // The number of 100 nanosecond intervals from Jan 1, 1601 'til Jan 1, 1970.
  const int64 kOffset = GG_LONGLONG(116444736000000000);
  timespec result;
  result.tv_sec = (n.QuadPart - kOffset) / 10000000;
  result.tv_nsec = (n.QuadPart - kOffset -
                    (result.tv_sec * GG_LONGLONG(10000000))) * 100;
  return result;
#else
  struct timeval now;
  struct timezone zone;
  gettimeofday(&now, &zone);
  struct timespec deadline = { now.tv_sec };
  // microseconds to nanoseconds.
  // and add the ms delay.
  ms += now.tv_usec / 1000;
  deadline.tv_sec += ms / 1000;
  deadline.tv_nsec = (ms % 1000) * 1000000;
  return deadline;
#endif
}

void NameCurrentThreadForDebugging(char* name) {
#if defined(OS_WINDOWS)
  // This implementation is taken from Chromium's platform_thread framework.
  // The information on how to set the thread name comes from a MSDN article:
  // http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
  const DWORD kVCThreadNameException = 0x406D1388;
  typedef struct tagTHREADNAME_INFO {
    DWORD dwType;  // Must be 0x1000.
    LPCSTR szName;  // Pointer to name (in user addr space).
    DWORD dwThreadID;  // Thread ID (-1=caller thread).
    DWORD dwFlags;  // Reserved for future use, must be zero.
  } THREADNAME_INFO;

  // The debugger needs to be around to catch the name in the exception. If
  // there isn't a debugger, we are just needlessly throwing an exception.
  if (!::IsDebuggerPresent())
    return;

  THREADNAME_INFO info = { 0x1000, name, GetCurrentThreadId(), 0 };

  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
#endif  // defined(OS_WINDOWS)
}
