// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/features.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/zlib.h"

// Some builds don't support the allocator shim in which case the memory long
// won't function.
#if BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace {

class MemlogBrowserTest : public InProcessBrowserTest,
                          public testing::WithParamInterface<const char*> {
 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    if (GetParam())
      command_line->AppendSwitchASCII(switches::kMemlog, GetParam());
  }
};

void ValidateDump(base::Value* dump_json,
                  int expected_alloc_size,
                  int expected_alloc_count,
                  const char* allocator_name,
                  const char* type_name) {
  // Verify allocation is found.
  // See chrome/profiling/json_exporter.cc for file format info.
  base::Value* events = dump_json->FindKey("traceEvents");
  base::Value* dumps = nullptr;
  for (base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() == "periodic_interval") {
      dumps = &event;
      break;
    }
  }
  ASSERT_TRUE(dumps);

  base::Value* heaps_v2 = dumps->FindPath({"args", "dumps", "heaps_v2"});
  ASSERT_TRUE(heaps_v2);

  base::Value* sizes =
      heaps_v2->FindPath({"allocators", allocator_name, "sizes"});
  ASSERT_TRUE(sizes);
  const base::Value::ListStorage& sizes_list = sizes->GetList();
  EXPECT_FALSE(sizes_list.empty());

  base::Value* counts =
      heaps_v2->FindPath({"allocators", allocator_name, "counts"});
  ASSERT_TRUE(counts);
  const base::Value::ListStorage& counts_list = counts->GetList();
  EXPECT_EQ(sizes_list.size(), counts_list.size());

  base::Value* types =
      heaps_v2->FindPath({"allocators", allocator_name, "types"});
  ASSERT_TRUE(types);
  const base::Value::ListStorage& types_list = types->GetList();
  EXPECT_FALSE(types_list.empty());
  EXPECT_EQ(sizes_list.size(), types_list.size());

  bool found_browser_alloc = false;
  size_t browser_alloc_index = 0;
  for (size_t i = 0; i < sizes_list.size(); i++) {
    if (counts_list[i].GetInt() == expected_alloc_count &&
        sizes_list[i].GetInt() != expected_alloc_size) {
      LOG(WARNING) << "Allocation candidate (size:" << sizes_list[i].GetInt()
                   << " count:" << counts_list[i].GetInt() << ")";
    }
    if (sizes_list[i].GetInt() == expected_alloc_size &&
        counts_list[i].GetInt() == expected_alloc_count) {
      browser_alloc_index = i;
      found_browser_alloc = true;
      break;
    }
  }

  ASSERT_TRUE(found_browser_alloc)
      << "Failed to find an allocation of the "
         "appropriate size. Did the send buffer "
         "not flush? (size: "
      << expected_alloc_size << " count:" << expected_alloc_count << ")";

  // Find the type, if an expectation was passed in.
  if (type_name) {
    bool found = false;
    int type = types_list[browser_alloc_index].GetInt();
    base::Value* strings = heaps_v2->FindPath({"maps", "strings"});
    for (base::Value& dict : strings->GetList()) {
      // Each dict has the format {"id":1,"string":"kPartitionAllocTypeName"}
      int id = dict.FindPath({"id"})->GetInt();
      if (id == type) {
        found = true;
        std::string name = dict.FindPath({"string"})->GetString();
        EXPECT_STREQ(name.c_str(), type_name);
        break;
      }
    }
    EXPECT_TRUE(found) << "Failed to find type name string.";
  }
}

std::unique_ptr<base::Value> ReadDumpFile(const base::FilePath& path) {
  using base::File;
  File dumpfile(path, File::FLAG_OPEN | File::FLAG_READ);

#if defined(OS_WIN)
  int fd = _open_osfhandle(
      reinterpret_cast<intptr_t>(dumpfile.TakePlatformFile()), 0);
#else
  int fd = dumpfile.TakePlatformFile();
#endif
  gzFile gz_file = gzdopen(fd, "r");
  if (!gz_file) {
    LOG(ERROR) << "Cannot open compressed trace file";
    return nullptr;
  }

  std::string dump_string;

  char buf[4096];
  int bytes_read;
  while ((bytes_read = gzread(gz_file, buf, sizeof(buf))) == sizeof(buf)) {
    dump_string.append(buf, bytes_read);
  }
  if (bytes_read < 0) {
    LOG(ERROR) << "Error reading file";
    return nullptr;
  }
  dump_string.append(buf, bytes_read);  // Grab last bytes.
  if (dump_string.size() == 0) {
    // Not an error if there's no data.
    return nullptr;
  }

  return base::JSONReader::Read(dump_string);
}

void DumpProcess(base::ProcessId pid, const base::FilePath& dumpfile_path) {
  profiling::ProfilingProcessHost* pph =
      profiling::ProfilingProcessHost::GetInstance();
  base::RunLoop run_loop;
  pph->RequestProcessDump(
      pid, dumpfile_path,
      base::BindOnce(&base::RunLoop::Quit, base::Unretained(&run_loop)));
  run_loop.Run();
}

// Test that we log the right processes and that the data goes into the dump
// file. Specifically:
//   1) Starts the memlog process
//   2) Makes a bunch of allocations
//   3) Performs a heap dump.
//   4) Checks that the allocations from (2) made it into (4).
IN_PROC_BROWSER_TEST_P(MemlogBrowserTest, EndToEnd) {
  if (!GetParam()) {
    // Test that nothing has been started if the flag is not passed. Then early
    // exit.
    ASSERT_FALSE(profiling::ProfilingProcessHost::has_started());
    return;
  } else {
    ASSERT_TRUE(profiling::ProfilingProcessHost::has_started());
  }

  // Create directory for storing dumps.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Do a dummy dump of the browser process as a way to synchronize the test
  // on profiling process start.
  DumpProcess(
      base::Process::Current().Pid(),
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("throwaway.json.gz")));

  // Make some specific allocations in Browser to do a deeper test of the
  // allocation tracking. On the renderer side, this is harder so all that's
  // tested there is the existence of information.
  constexpr int kBrowserAllocSize = 103 * 1024;
  constexpr int kBrowserAllocCount = 2048;

  // Test fixed-size partition alloc. The size must be aligned to system pointer
  // size.
  constexpr int kPartitionAllocSize = 8 * 23;
  constexpr int kPartitionAllocCount = 107;
  static const char* kPartitionAllocTypeName = "kPartitionAllocTypeName";
  base::PartitionAllocatorGeneric partition_allocator;
  partition_allocator.init();

  std::vector<char*> leaks;
  leaks.reserve(2 * kBrowserAllocCount + kPartitionAllocSize);
  for (int i = 0; i < kBrowserAllocCount; ++i) {
    leaks.push_back(new char[kBrowserAllocSize]);
  }

  for (int i = 0; i < kPartitionAllocCount; ++i) {
    leaks.push_back(static_cast<char*>(
        PartitionAllocGeneric(partition_allocator.root(), kPartitionAllocSize,
                              kPartitionAllocTypeName)));
  }

  size_t total_variadic_allocations = 0;
  for (int i = 0; i < kBrowserAllocCount; ++i) {
    leaks.push_back(new char[i + 1]);  // Variadic allocation.
    total_variadic_allocations += i + 1;
  }

  // Navigate around to force allocations in the renderer.
  // Also serves to give a lot of time for the browser allocations to propagate
  // through to the profiling process.
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/english_page.html"));
  // Vive la France!
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/french_page.html"));

  {
    SCOPED_TRACE("Validating Browser Allocation");
    base::FilePath browser_dumpfile_path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("browserdump.json.gz"));
    DumpProcess(base::Process::Current().Pid(), browser_dumpfile_path);

    std::unique_ptr<base::Value> dump_json =
        ReadDumpFile(browser_dumpfile_path);
    ASSERT_TRUE(dump_json);
    ASSERT_NO_FATAL_FAILURE(
        ValidateDump(dump_json.get(), kBrowserAllocSize * kBrowserAllocCount,
                     kBrowserAllocCount, "malloc", nullptr));
    ASSERT_NO_FATAL_FAILURE(
        ValidateDump(dump_json.get(), total_variadic_allocations,
                     kBrowserAllocCount, "malloc", nullptr));
    ASSERT_NO_FATAL_FAILURE(ValidateDump(
        dump_json.get(), kPartitionAllocSize * kPartitionAllocCount,
        kPartitionAllocCount, "partition_alloc", kPartitionAllocTypeName));
  }

  {
    SCOPED_TRACE("Validating Renderer Allocation");
    base::FilePath renderer_dumpfile_path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("rendererdump.json.gz"));
    DumpProcess(base::GetProcId(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetMainFrame()
                                    ->GetProcess()
                                    ->GetHandle()),
                renderer_dumpfile_path);
    std::unique_ptr<base::Value> dump_json =
        ReadDumpFile(renderer_dumpfile_path);
    if (GetParam() == switches::kMemlogModeAll) {
      ASSERT_TRUE(dump_json);

      // ValidateDump doesn't always succeed for the renderer, since we don't do
      // anything to flush allocations, there are very few allocations recorded
      // by the heap profiler. When we do a heap dump, we prune small
      // allocations...and this can cause all allocations to be pruned.
      // ASSERT_NO_FATAL_FAILURE(ValidateDump(dump_json.get(), 0, 0));
    } else {
      ASSERT_FALSE(dump_json)
          << "Renderer should not be dumpable unless kMemlogModeAll!";
    }
  }
}

// TODO(ajwong): Test what happens if profiling process crashes.
// TODO(ajwong): Test the pure json output path used by tracing.

INSTANTIATE_TEST_CASE_P(NoMemlog,
                        MemlogBrowserTest,
                        ::testing::Values(static_cast<const char*>(nullptr)));
INSTANTIATE_TEST_CASE_P(BrowserOnly,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeMinimal));
INSTANTIATE_TEST_CASE_P(AllProcesses,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeAll));

}  // namespace

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
