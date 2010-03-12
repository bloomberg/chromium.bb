// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_NT_LOADER_H_
#define CHROME_FRAME_NT_LOADER_H_

#include <windows.h>
#include <winnt.h>
#include <winternl.h>

namespace nt_loader {

// These structures are gleaned from public symbol information.
struct _PEB;
struct _PEB_LDR_DATA;
struct _RTL_USER_PROCESS_PARAMETERS;
struct _PEB_FREE_BLOCK;


typedef struct _NT_TIB {
  _EXCEPTION_REGISTRATION_RECORD* ExceptionList;  // 0x000
  void *StackBase;  // 0x004
  void* StackLimit;  // 0x008
  void* SubSystemTib;  // 0x00c
  union {
    void *FiberData;  // 0x010
    DWORD Version;  // 0x010
  };
  void* ArbitraryUserPointer;  // 0x014
  _NT_TIB* Self;  // 0x018
} _NT_TIB, NT_TIB;

typedef struct _CLIENT_ID {
  void* UniqueProcess;  // 0x000
  void* UniqueThread;  // 0x004
} _CLIENT_ID, CLIENT_ID;

typedef struct _TEB {
  _NT_TIB NtTib;  // 0x000
  void* EnvironmentPointer;  // 0x01c
  _CLIENT_ID ClientId;  // 0x020
  void* ActiveRpcHandle;  // 0x028
  void* ThreadLocalStoragePointer;  // 0x02c
  _PEB* ProcessEnvironmentBlock;  // 0x030
  // There is more in a TEB, but this is all we need.
} _TEB, TEB;

typedef struct _PEB {
  BYTE InheritedAddressSpace;  // 0x000
  BYTE ReadImageFileExecOptions;  // 0x001
  BYTE BeingDebugged;  // 0x002
  BYTE SpareBool;  // 0x003
  void* Mutant;  // 0x004
  void* ImageBaseAddress;  // 0x008
  _PEB_LDR_DATA* Ldr;  // 0x00c
  _RTL_USER_PROCESS_PARAMETERS* ProcessParameters;  // 0x010
  void* SubSystemData;  // 0x014
  void* ProcessHeap;  // 0x018
  _RTL_CRITICAL_SECTION* FastPebLock;  // 0x01c
  void* FastPebLockRoutine;  // 0x020
  void* FastPebUnlockRoutine;  // 0x024
  ULONG EnvironmentUpdateCount;  // 0x028
  void* KernelCallbackTable;  // 0x02c
  ULONG SystemReserved[1]; // 0x030
  ULONG AtlThunkSListPtr32;  // 0x034
  _PEB_FREE_BLOCK* FreeList;  // 0x038
  ULONG TlsExpansionCounter;  // 0x03c
  void* TlsBitmap;  // 0x040
  ULONG TlsBitmapBits[2]; // 0x044
  void* ReadOnlySharedMemoryBase;  // 0x04c
  void* ReadOnlySharedMemoryHeap;  // 0x050
  void** ReadOnlyStaticServerData;  // 0x054
  void* AnsiCodePageData;  // 0x058
  void* OemCodePageData;  // 0x05c
  void* UnicodeCaseTableData;  // 0x060
  ULONG NumberOfProcessors;  // 0x064
  ULONG NtGlobalFlag;  // 0x068
  _LARGE_INTEGER CriticalSectionTimeout;  // 0x070
  ULONG HeapSegmentReserve;  // 0x078
  ULONG HeapSegmentCommit;  // 0x07c
  ULONG HeapDeCommitTotalFreeThreshold;  // 0x080
  ULONG HeapDeCommitFreeBlockThreshold;  // 0x084
  ULONG NumberOfHeaps;  // 0x088
  ULONG MaximumNumberOfHeaps;  // 0x08c
  void** ProcessHeaps;  // 0x090
  void* GdiSharedHandleTable;  // 0x094
  void* ProcessStarterHelper;  // 0x098
  ULONG GdiDCAttributeList;  // 0x09c
  RTL_CRITICAL_SECTION* LoaderLock;  // 0x0a0
  // There is more in a PEB, but this is all we need.
} _PEB, PEB;

struct _PEB_LDR_DATA {
  ULONG Length;  // 0x000
  BYTE Initialized;  // 0x004
  void* SsHandle;  // 0x008
  LIST_ENTRY InLoadOrderModuleList;  // 0x00c
  LIST_ENTRY InMemoryOrderModuleList;  // 0x014
  LIST_ENTRY InInitializationOrderModuleList;  // 0x01c
  // There is more data in this structure, but this is all we need.
};

// These flags are gleaned from the !dlls Windbg extension.
#define LDRP_STATIC_LINK            0x00000002
#define LDRP_IMAGE_DLL              0x00000004
#define LDRP_LOAD_IN_PROGRESS       0x00001000
#define LDRP_UNLOAD_IN_PROGRESS     0x00002000
#define LDRP_ENTRY_PROCESSED        0x00004000
#define LDRP_DONT_CALL_FOR_THREADS  0x00040000
#define LDRP_PROCESS_ATTACH_CALLED  0x00080000
#define LDRP_COR_IMAGE              0x00400000
#define LDRP_COR_OWNS_UNMAP         0x00800000
#define LDRP_COR_IL_ONLY            0x01000000
#define LDRP_REDIRECTED             0x10000000

typedef struct _LDR_DATA_TABLE_ENTRY {
  LIST_ENTRY InLoadOrderLinks;  // 0x000
  LIST_ENTRY InMemoryOrderLinks;  // 0x008
  LIST_ENTRY InInitializationOrderLinks; //  0x010
  void* DllBase;  // 0x018
  void* EntryPoint;  // 0x01c
  ULONG SizeOfImage;  // 0x020
  UNICODE_STRING FullDllName;  // 0x024
  UNICODE_STRING BaseDllName;  // 0x02c
  ULONG Flags;  // 0x034
  USHORT LoadCount;  // 0x038
  USHORT TlsIndex;  // 0x03a
  union {
    LIST_ENTRY HashLinks;  // 0x03c
    struct {
      void* SectionPointer;  // 0x03c
      ULONG CheckSum;  // 0x040
    };
  };
  union {
    ULONG TimeDateStamp;  // 0x044
    void* LoadedImports;  // 0x044
  };
  void *EntryPointActivationContext;  // 0x048
  void* PatchInformation;  // 0x04c
} _LDR_DATA_TABLE_ENTRY, LDR_DATA_TABLE_ENTRY;

// Retrieves the current thread's TEB.
inline TEB* GetCurrentTeb() {
  return reinterpret_cast<TEB*>(NtCurrentTeb());
}

// Retrieves the current process' PEB.
inline PEB* GetCurrentPeb() {
  return GetCurrentTeb()->ProcessEnvironmentBlock;
}

// Returns true iff the current thread owns critsec.
inline bool OwnsCriticalSection(CRITICAL_SECTION* critsec) {
  return reinterpret_cast<DWORD>(critsec->OwningThread) ==
      GetCurrentThreadId();
}

// Finds a loader table entry for module.
// Note: must hold the loader's lock on entry.
LDR_DATA_TABLE_ENTRY* GetLoaderEntry(HMODULE module);

// Returns the loader's lock.
inline CRITICAL_SECTION* GetLoaderLock() {
  return GetCurrentPeb()->LoaderLock;
}

// Returns true iff the current thread owns the loader's lock on call.
inline bool OwnsLoaderLock() {
  return OwnsCriticalSection(GetLoaderLock());
}

}  // namespace nt_loader

#endif  // CHROME_FRAME_NT_LOADER_H_
