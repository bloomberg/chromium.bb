// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NT internal data structures and functions that rely upon them.
#ifndef CEEE_TESTING_UTILS_NT_INTERNALS_H_
#define CEEE_TESTING_UTILS_NT_INTERNALS_H_

#include <windows.h>

namespace nt_internals {

// The structures, constants and function declarations in here are gleaned
// from various sources, primarily from Microsoft symbol information for
// system DLLs, kernel modules and executables, as well as from MSDN help.
// For more information on the Windows NT native API, see e.g.
// the "Windows NT/2000 Native API Reference" by Gary Nebbett,
// ISBN 9781578701995.
typedef LONG NTSTATUS;
typedef DWORD KPRIORITY;
typedef WORD UWORD;

struct CLIENT_ID {
  PVOID UniqueProcess;
  PVOID UniqueThread;
};

struct THREAD_BASIC_INFO {
  NTSTATUS                ExitStatus;
  PVOID                   TebBaseAddress;
  CLIENT_ID               ClientId;
  KAFFINITY               AffinityMask;
  KPRIORITY               Priority;
  KPRIORITY               BasePriority;
};

enum THREADINFOCLASS {
  ThreadBasicInformation,
};

// NtQueryInformationProcess is documented in winternl.h, but we link
// dynamically to it because we might as well.
typedef NTSTATUS (WINAPI *NtQueryInformationThreadFunc)(
                  IN HANDLE ThreadHandle,
                  IN THREADINFOCLASS ThreadInformationClass,
                  OUT PVOID ThreadInformation,
                  IN ULONG ThreadInformationLength,
                  OUT PULONG ReturnLength OPTIONAL);
extern NtQueryInformationThreadFunc NtQueryInformationThread;

typedef struct _PROCESS_BASIC_INFORMATION {
  NTSTATUS          ExitStatus;
  PVOID             PebBaseAddress;
  ULONG             AffinityMask;
  ULONG             BasePriority;
  ULONG_PTR         UniqueProcessId;
  ULONG_PTR         InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

enum PROCESSINFOCLASS {
  ProcessBasicInformation
};

typedef NTSTATUS (WINAPI *NtQueryInformationProcessFunc)(
                  IN HANDLE ProcessHandle,
                  IN PROCESSINFOCLASS ProcessInformationClass,
                  OUT PVOID ProcessInformation,
                  IN ULONG ProcessInformationLength,
                  OUT PULONG ReturnLength OPTIONAL);
extern NtQueryInformationProcessFunc NtQueryInformationProcess;

// NtQueryObject is also documented in winternl.h, but we link
// dynamically to it because we might as well.
typedef struct _PUBLIC_OBJECT_BASIC_INFORMATION {
  ULONG       Attributes;
  ACCESS_MASK GrantedAccess;
  ULONG       HandleCount;
  ULONG       PointerCount;
  ULONG       Reserved[10];    // reserved for internal use
} PUBLIC_OBJECT_BASIC_INFORMATION;

enum OBJECT_INFORMATION_CLASS {
  ObjectBasicInformation = 0,
  ObjectTypeInformation  = 2,
};

typedef NTSTATUS (WINAPI *NtQueryObjectFunc)(
                  IN HANDLE Handle,
                  IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
                  OUT PVOID ObjectInformation,
                  IN ULONG ObjectInformationLength,
                  OUT PULONG ReturnLength);
extern NtQueryObjectFunc NtQueryObject;

// @returns the TIB/TEB address for "thread".
extern PNT_TIB GetThreadTIB(HANDLE thread);

// Debug-related structure declarations.
typedef struct _RTL_PROCESS_BACKTRACE_INFORMATION {
  PVOID SymbolicBackTrace;
  ULONG TraceCount;
  USHORT Index;
  USHORT Depth;
  PVOID BackTrace[32];
} RTL_PROCESS_BACKTRACE_INFORMATION, *PRTL_PROCESS_BACKTRACE_INFORMATION;

typedef struct _RTL_PROCESS_BACKTRACES {
  ULONG CommittedMemory;
  ULONG ReservedMemory;
  ULONG NumberOfBackTraceLookups;
  ULONG NumberOfBackTraces;
  RTL_PROCESS_BACKTRACE_INFORMATION BackTraces[1];
} RTL_PROCESS_BACKTRACES, *PRTL_PROCESS_BACKTRACES;

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
  ULONG Section;
  PVOID MappedBase;
  PVOID ImageBase;
  ULONG ImageSize;
  ULONG Flags;
  USHORT LoadOrderIndex;
  USHORT InitOrderIndex;
  USHORT LoadCount;
  USHORT OffsetToFileName;
  CHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
  ULONG NumberOfModules;
  RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef struct _RTL_PROCESS_MODULE_INFORMATION_EX {
  ULONG NextOffset;
  RTL_PROCESS_MODULE_INFORMATION BaseInfo;
  ULONG ImageCheckSum;
  ULONG TimeDateStamp;
  PVOID DefaultBase;
} RTL_PROCESS_MODULE_INFORMATION_EX, *PRTL_PROCESS_MODULE_INFORMATION_EX;

typedef struct _RTL_DEBUG_INFORMATION {
  HANDLE SectionHandleClient;
  PVOID ViewBaseClient;
  PVOID ViewBaseTarget;
  ULONG ViewBaseDelta;
  HANDLE EventPairClient;
  PVOID EventPairTarget;
  HANDLE TargetProcessId;
  HANDLE TargetThreadHandle;
  ULONG Flags;
  ULONG OffsetFree;
  ULONG CommitSize;
  ULONG ViewSize;
  union {
    PRTL_PROCESS_MODULES Modules;
    PRTL_PROCESS_MODULE_INFORMATION_EX ModulesEx;
  };
  PRTL_PROCESS_BACKTRACES BackTraces;
  void * /*PRTL_PROCESS_HEAPS*/ Heaps;
  void * /*PRTL_PROCESS_LOCKS*/ Locks;
  HANDLE SpecificHeap;
  HANDLE TargetProcessHandle;
  void * /*RTL_PROCESS_VERIFIER_OPTIONS*/ VerifierOptions;
  HANDLE ProcessHeap;
  HANDLE CriticalSectionHandle;
  HANDLE CriticalSectionOwnerThread;
  PVOID Reserved[4];
} RTL_DEBUG_INFORMATION, *PRTL_DEBUG_INFORMATION;

// RtlQueryProcessDebugInformation.DebugInfoClassMask constants
#define PDI_MODULES 0x01
#define PDI_BACKTRACE 0x02
#define PDI_HEAPS 0x04
#define PDI_HEAP_TAGS 0x08
#define PDI_HEAP_BLOCKS 0x10
#define PDI_LOCKS 0x20

typedef NTSTATUS (NTAPI *RtlQueryProcessBackTraceInformationFunc)(
      PRTL_DEBUG_INFORMATION debug_info);
typedef PRTL_DEBUG_INFORMATION (NTAPI *RtlCreateQueryDebugBufferFunc)(
      IN ULONG size, IN BOOLEAN event_pair);
typedef NTSTATUS (NTAPI *RtlQueryProcessDebugInformationFunc)(
      IN ULONG ProcessId, IN ULONG DebugInfoClassMask,
      IN OUT PRTL_DEBUG_INFORMATION DebugBuffer);
typedef NTSTATUS (NTAPI *RtlDestroyQueryDebugBufferFunc)(
      IN PRTL_DEBUG_INFORMATION DebugBuffer);

typedef ULONG (NTAPI *RtlGetNtGlobalFlagsFunc)();
typedef ULONG (NTAPI *RtlNtStatusToDosErrorFunc)(NTSTATUS nt_status);

// Debug-related runtime functions.
extern RtlQueryProcessBackTraceInformationFunc
    RtlQueryProcessBackTraceInformation;
extern RtlCreateQueryDebugBufferFunc
    RtlCreateQueryDebugBuffer;
extern RtlQueryProcessDebugInformationFunc
    RtlQueryProcessDebugInformation;
extern RtlDestroyQueryDebugBufferFunc
    RtlDestroyQueryDebugBuffer;

extern RtlGetNtGlobalFlagsFunc RtlGetNtGlobalFlags;
extern RtlNtStatusToDosErrorFunc RtlNtStatusToDosError;

}  // namespace nt_internals

#endif  // CEEE_TESTING_UTILS_NT_INTERNALS_H_
