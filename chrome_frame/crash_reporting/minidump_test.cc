// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <dbghelp.h>
#include <objbase.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "gtest/gtest.h"

namespace {

// Convenience to get to the PEB pointer in a TEB.
struct FakeTEB {
  char dummy[0x30];
  void* peb;
};

// Minidump with stacks, PEB, TEB, and unloaded module list.
const MINIDUMP_TYPE kSmallDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

// Minidump with all of the above, plus memory referenced from stack.
const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

// Large dump with all process memory.
const MINIDUMP_TYPE kFullDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithFullMemory |  // Full memory from process.
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithHandleData |  // Get all handle information.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.


class MinidumpTest: public testing::Test {
 public:
  MinidumpTest() : dump_file_view_(NULL) {
  }

  virtual void SetUp() {
    // Make sure URLMon isn't loaded into our process.
    ASSERT_EQ(NULL, ::GetModuleHandle(L"urlmon.dll"));

    // Then load and unload it to ensure we have something to
    // stock the unloaded module list with.
    HMODULE urlmon = ::LoadLibrary(L"urlmon.dll");
    ASSERT_TRUE(urlmon != NULL);
    ASSERT_TRUE(::FreeLibrary(urlmon));

    ASSERT_TRUE(file_util::CreateTemporaryFile(&dump_file_));
    dump_file_handle_.Set(::CreateFile(dump_file_.value().c_str(),
                                       GENERIC_WRITE | GENERIC_READ,
                                       0,
                                       NULL,
                                       OPEN_EXISTING,
                                       0,
                                       NULL));
    ASSERT_TRUE(dump_file_handle_.IsValid());
  }

  virtual void TearDown() {
    if (dump_file_view_ != NULL) {
      EXPECT_TRUE(::UnmapViewOfFile(dump_file_view_));
      dump_file_mapping_.Close();
    }

    dump_file_handle_.Close();
    EXPECT_TRUE(file_util::Delete(dump_file_, false));
  }

  void EnsureDumpMapped() {
    ASSERT_TRUE(dump_file_handle_.IsValid());
    if (dump_file_view_ == NULL) {
      ASSERT_FALSE(dump_file_mapping_.IsValid());

      dump_file_mapping_.Set(::CreateFileMapping(dump_file_handle_,
                                                 NULL,
                                                 PAGE_READONLY,
                                                 0,
                                                 0,
                                                 NULL));
      ASSERT_TRUE(dump_file_mapping_.IsValid());

      dump_file_view_ = ::MapViewOfFile(dump_file_mapping_.Get(),
                                        FILE_MAP_READ,
                                        0,
                                        0,
                                        0);
      ASSERT_TRUE(dump_file_view_ != NULL);
    }
  }

  bool WriteDump(ULONG flags) {
    // Fake exception is access violation on write to this.
    EXCEPTION_RECORD ex_record = {
        STATUS_ACCESS_VIOLATION,  // ExceptionCode
        0,  // ExceptionFlags
        NULL,  // ExceptionRecord;
        reinterpret_cast<void*>(0xCAFEBABE), // ExceptionAddress;
        2,  // NumberParameters;
        { EXCEPTION_WRITE_FAULT, reinterpret_cast<ULONG_PTR>(this) }
    };
    CONTEXT ctx_record = {};
    EXCEPTION_POINTERS ex_ptrs = {
      &ex_record,
      &ctx_record,
    };
    MINIDUMP_EXCEPTION_INFORMATION ex_info = {
        ::GetCurrentThreadId(),
        &ex_ptrs,
        FALSE,
    };

    // Capture our register context.
    ::RtlCaptureContext(&ctx_record);

    // And write a dump
    BOOL result = ::MiniDumpWriteDump(::GetCurrentProcess(),
                                      ::GetCurrentProcessId(),
                                      dump_file_handle_.Get(),
                                      static_cast<MINIDUMP_TYPE>(flags),
                                      &ex_info,
                                      NULL,
                                      NULL);
    VLOG(1) << "Flags: " << flags << " mindump size: "
            << ::GetFileSize(dump_file_handle_.Get(), NULL);

    return result == TRUE;
  }

  bool DumpHasStream(ULONG stream_number) {
    EnsureDumpMapped();

    MINIDUMP_DIRECTORY* directory = NULL;
    void* stream = NULL;
    ULONG stream_size = 0;
    BOOL ret = ::MiniDumpReadDumpStream(dump_file_view_,
                                        stream_number,
                                        &directory,
                                        &stream,
                                        &stream_size);

    return ret != FALSE && stream != NULL && stream_size > 0;
  }

  template <class StreamType>
  size_t GetStream(ULONG stream_number, StreamType** stream) {
    EnsureDumpMapped();
    MINIDUMP_DIRECTORY* directory = NULL;
    ULONG memory_list_size = 0;
    BOOL ret = ::MiniDumpReadDumpStream(dump_file_view_,
                                        stream_number,
                                        &directory,
                                        reinterpret_cast<void**>(stream),
                                        &memory_list_size);

    return ret ? memory_list_size : 0;
  }

  bool DumpHasTebs() {
    MINIDUMP_THREAD_LIST* thread_list = NULL;
    size_t thread_list_size = GetStream(ThreadListStream, &thread_list);

    if (thread_list_size > 0 && thread_list != NULL) {
      for (ULONG i = 0; i < thread_list->NumberOfThreads; ++i) {
        if (!DumpHasMemory(thread_list->Threads[i].Teb))
          return false;
      }

      return true;
    }

    // No thread list, no TEB info.
    return false;
  }

  bool DumpHasPeb() {
    MINIDUMP_THREAD_LIST* thread_list = NULL;
    size_t thread_list_size = GetStream(ThreadListStream, &thread_list);

    if (thread_list_size > 0 && thread_list != NULL &&
        thread_list->NumberOfThreads > 0) {
      FakeTEB* teb = NULL;
      if (!DumpHasMemory(thread_list->Threads[0].Teb, &teb))
        return false;

      return DumpHasMemory(teb->peb);
    }

    return false;
  }

  bool DumpHasMemory(ULONG64 address) {
    return DumpHasMemory<uint8>(address, NULL);
  }

  bool DumpHasMemory(const void* address) {
    return DumpHasMemory<uint8>(address, NULL);
  }

  template <class StructureType>
  bool DumpHasMemory(ULONG64 address, StructureType** structure = NULL) {
    // We can't cope with 64 bit addresses for now.
    if (address > 0xFFFFFFFFUL)
      return false;

    return DumpHasMemory(reinterpret_cast<void*>(address), structure);
  }

  template <class StructureType>
  bool DumpHasMemory(const void* addr_in, StructureType** structure = NULL) {
    uintptr_t address = reinterpret_cast<uintptr_t>(addr_in);
    MINIDUMP_MEMORY_LIST* memory_list = NULL;
    size_t memory_list_size = GetStream(MemoryListStream, &memory_list);
    if (memory_list_size > 0 && memory_list != NULL) {
      for (ULONG i = 0; i < memory_list->NumberOfMemoryRanges; ++i) {
        MINIDUMP_MEMORY_DESCRIPTOR& descr = memory_list->MemoryRanges[i];
        const uintptr_t range_start =
            static_cast<uintptr_t>(descr.StartOfMemoryRange);
        uintptr_t range_end = range_start + descr.Memory.DataSize;

        if (address >= range_start &&
            address + sizeof(StructureType) < range_end) {
          // The start address falls in the range, and the end address is
          // in bounds, return a pointer to the structure if requested.
          if (structure != NULL)
            *structure = reinterpret_cast<StructureType*>(
                RVA_TO_ADDR(dump_file_view_, descr.Memory.Rva));

          return true;
        }
      }
    }

    // We didn't find the range in a MINIDUMP_MEMORY_LIST, so maybe this
    // is a full dump using MINIDUMP_MEMORY64_LIST with all the memory at the
    // end of the dump file.
    MINIDUMP_MEMORY64_LIST* memory64_list = NULL;
    memory_list_size = GetStream(Memory64ListStream, &memory64_list);
    if (memory_list_size > 0 && memory64_list != NULL) {
      // Keep track of where the current descriptor maps to.
      RVA64 curr_rva = memory64_list->BaseRva;
      for (ULONG i = 0; i < memory64_list->NumberOfMemoryRanges; ++i) {
        MINIDUMP_MEMORY_DESCRIPTOR64& descr = memory64_list->MemoryRanges[i];
        uintptr_t range_start =
            static_cast<uintptr_t>(descr.StartOfMemoryRange);
        uintptr_t range_end = range_start + static_cast<size_t>(descr.DataSize);

        if (address >= range_start &&
            address + sizeof(StructureType) < range_end) {
          // The start address falls in the range, and the end address is
          // in bounds, return a pointer to the structure if requested.
          if (structure != NULL)
            *structure = reinterpret_cast<StructureType*>(
                RVA_TO_ADDR(dump_file_view_, curr_rva));

          return true;
        }

        // Advance the current RVA.
        curr_rva += descr.DataSize;
      }
    }



    return false;
  }

 protected:
  base::win::ScopedHandle dump_file_handle_;
  base::win::ScopedHandle dump_file_mapping_;
  void* dump_file_view_;

  base::FilePath dump_file_;
};

TEST_F(MinidumpTest, Version) {
  API_VERSION* version = ::ImagehlpApiVersion();

  VLOG(1) << "Imagehlp Api Version: " << version->MajorVersion << "."
          << version->MinorVersion << "." << version->Revision;

  HMODULE dbg_help = ::GetModuleHandle(L"dbghelp.dll");
  ASSERT_TRUE(dbg_help != NULL);

  wchar_t dbg_help_file[1024] = {};
  ASSERT_TRUE(::GetModuleFileName(dbg_help,
                                  dbg_help_file,
                                  arraysize(dbg_help_file)));
  scoped_ptr<FileVersionInfo> file_info(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(dbg_help_file)));
  ASSERT_TRUE(file_info != NULL);

  VLOG(1) << "DbgHelp.dll version: " << file_info->file_version();
}

TEST_F(MinidumpTest, Normal) {
  EXPECT_TRUE(WriteDump(MiniDumpNormal));

  // We expect threads, modules and some memory.
  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(MemoryListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(Memory64ListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(HandleDataStream));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));

  // We expect no PEB nor TEBs in this dump.
  EXPECT_FALSE(DumpHasTebs());
  EXPECT_FALSE(DumpHasPeb());

  // We expect no off-stack memory in this dump.
  EXPECT_FALSE(DumpHasMemory(this));
}

TEST_F(MinidumpTest, SmallDump) {
  ASSERT_TRUE(WriteDump(kSmallDumpType));

  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(MemoryListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));

  // We expect PEB and TEBs in this dump.
  EXPECT_TRUE(DumpHasTebs());
  EXPECT_TRUE(DumpHasPeb());

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(Memory64ListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(HandleDataStream));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));

  // We expect no off-stack memory in this dump.
  EXPECT_FALSE(DumpHasMemory(this));
}

TEST_F(MinidumpTest, LargerDump) {
  ASSERT_TRUE(WriteDump(kLargerDumpType));

  // The dump should have all of these streams.
  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(MemoryListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));

  // We expect memory referenced by stack in this dump.
  EXPECT_TRUE(DumpHasMemory(this));

  // We expect PEB and TEBs in this dump.
  EXPECT_TRUE(DumpHasTebs());
  EXPECT_TRUE(DumpHasPeb());

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(Memory64ListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(HandleDataStream));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));
}

TEST_F(MinidumpTest, FullDump) {
  ASSERT_TRUE(WriteDump(kFullDumpType));

  // The dump should have all of these streams.
  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(Memory64ListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));
  EXPECT_TRUE(DumpHasStream(HandleDataStream));

  // We expect memory referenced by stack in this dump.
  EXPECT_TRUE(DumpHasMemory(this));

  // We expect PEB and TEBs in this dump.
  EXPECT_TRUE(DumpHasTebs());
  EXPECT_TRUE(DumpHasPeb());

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(MemoryListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  CommandLine::Init(argc, argv);

  logging::InitLogging(
      L"CON",
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::DONT_LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  return RUN_ALL_TESTS();
}
