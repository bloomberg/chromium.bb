// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NT internal data structures and functions that rely upon them.
#include "ceee/testing/utils/nt_internals.h"

namespace nt_internals {

const HMODULE ntdll = ::GetModuleHandle(L"ntdll.dll");

NtQueryInformationThreadFunc NtQueryInformationThread =
    reinterpret_cast<NtQueryInformationThreadFunc>(
        ::GetProcAddress(ntdll, "NtQueryInformationThread"));

NtQueryInformationProcessFunc NtQueryInformationProcess =
    reinterpret_cast<NtQueryInformationProcessFunc>(
        ::GetProcAddress(ntdll, "NtQueryInformationProcess"));

NtQueryObjectFunc NtQueryObject = reinterpret_cast<NtQueryObjectFunc>(
    ::GetProcAddress(ntdll, "NtQueryObject"));

RtlGetNtGlobalFlagsFunc RtlGetNtGlobalFlags =
    reinterpret_cast<RtlGetNtGlobalFlagsFunc>(
        ::GetProcAddress(ntdll, "RtlGetNtGlobalFlags"));

RtlNtStatusToDosErrorFunc RtlNtStatusToDosError =
    reinterpret_cast<RtlNtStatusToDosErrorFunc>(
        ::GetProcAddress(ntdll, "RtlNtStatusToDosError"));

RtlQueryProcessBackTraceInformationFunc RtlQueryProcessBackTraceInformation =
    reinterpret_cast<RtlQueryProcessBackTraceInformationFunc>(
        ::GetProcAddress(ntdll, "RtlQueryProcessBackTraceInformation"));

RtlCreateQueryDebugBufferFunc RtlCreateQueryDebugBuffer =
    reinterpret_cast<RtlCreateQueryDebugBufferFunc>(
        ::GetProcAddress(ntdll, "RtlCreateQueryDebugBuffer"));
RtlQueryProcessDebugInformationFunc RtlQueryProcessDebugInformation =
    reinterpret_cast<RtlQueryProcessDebugInformationFunc>(
        ::GetProcAddress(ntdll, "RtlQueryProcessDebugInformation"));
RtlDestroyQueryDebugBufferFunc RtlDestroyQueryDebugBuffer =
    reinterpret_cast<RtlDestroyQueryDebugBufferFunc>(
        ::GetProcAddress(ntdll, "RtlDestroyQueryDebugBuffer"));


PNT_TIB GetThreadTIB(HANDLE thread) {
  if (!NtQueryInformationThread)
    return NULL;

  THREAD_BASIC_INFO info = {};
  ULONG ret_len = 0;
  NTSTATUS status = NtQueryInformationThread(thread,
                                             ThreadBasicInformation,
                                             &info,
                                             sizeof(info),
                                             &ret_len);
  if (0 != status || ret_len != sizeof(info))
    return NULL;

  return reinterpret_cast<PNT_TIB>(info.TebBaseAddress);
}

}  // namespace nt_internals
