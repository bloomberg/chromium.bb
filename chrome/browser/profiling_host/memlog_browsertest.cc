// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/features.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/profiling_host/profiling_test_driver.h"
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

namespace profiling {

namespace {

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

int NumProcessesWithName(base::Value* dump_json, std::string name) {
  base::Value* events = dump_json->FindKey("traceEvents");
  int num_found = 0;
  for (base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "process_name")
      continue;
    const base::Value* found_args =
        event.FindKeyOfType("args", base::Value::Type::DICTIONARY);
    if (!found_args)
      continue;
    const base::Value* found_process_name =
        found_args->FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_process_name)
      continue;
    if (found_process_name->GetString() != name)
      continue;
    num_found++;
  }

  return num_found;
}

base::Value* FindHeapsV2(base::ProcessId pid, base::Value* dump_json) {
  base::Value* events = dump_json->FindKey("traceEvents");
  base::Value* dumps = nullptr;
  base::Value* heaps_v2 = nullptr;
  for (base::Value& event : events->GetList()) {
    const base::Value* found_name =
        event.FindKeyOfType("name", base::Value::Type::STRING);
    if (!found_name)
      continue;
    if (found_name->GetString() != "periodic_interval")
      continue;
    const base::Value* found_pid =
        event.FindKeyOfType("pid", base::Value::Type::INTEGER);
    if (!found_pid)
      continue;
    if (static_cast<base::ProcessId>(found_pid->GetInt()) != pid)
      continue;
    dumps = &event;
    heaps_v2 = dumps->FindPath({"args", "dumps", "heaps_v2"});
    if (heaps_v2)
      return heaps_v2;
  }
  return nullptr;
}

// Verify expectations are present in heap dump.
void ValidateDump(base::Value* heaps_v2,
                  int expected_alloc_size,
                  int expected_alloc_count,
                  const char* allocator_name,
                  const char* type_name) {
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
      int id = dict.FindKey("id")->GetInt();
      if (id == type) {
        found = true;
        std::string name = dict.FindKey("string")->GetString();
        EXPECT_STREQ(name.c_str(), type_name);
        break;
      }
    }
    EXPECT_TRUE(found) << "Failed to find type name string.";
  }
}

}  // namespace

class MemlogBrowserTest : public InProcessBrowserTest,
                          public testing::WithParamInterface<const char*> {
 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    if (GetParam())
      command_line->AppendSwitchASCII(switches::kMemlog, GetParam());
  }

  void SetUp() override {
    // Force the renderer to be sampled in RendererSampling mode for
    // deterministic tests.
    if (GetParam() == switches::kMemlogModeRendererSampling) {
      profiling::ProfilingProcessHost::GetInstance()
          ->SetRendererSamplingAlwaysProfileForTest();
    }

    partition_allocator_.init();
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    // Intentionally avoid deallocations, since that will trigger a large number
    // of messages to be sent to the profiling process.
    leaks_.clear();
    InProcessBrowserTest::TearDown();
  }

  void MakeTestAllocations() {
    leaks_.reserve(2 * kBrowserAllocCount + kPartitionAllocSize);
    for (int i = 0; i < kBrowserAllocCount; ++i) {
      leaks_.push_back(new char[kBrowserAllocSize]);
    }

    for (int i = 0; i < kPartitionAllocCount; ++i) {
      leaks_.push_back(static_cast<char*>(partition_allocator_.root()->Alloc(
          kPartitionAllocSize, kPartitionAllocTypeName)));
    }

    for (int i = 0; i < kBrowserAllocCount; ++i) {
      leaks_.push_back(new char[i + 1]);  // Variadic allocation.
      total_variadic_allocations_ += i + 1;
    }

    // Navigate around to force allocations in the renderer.
    ASSERT_TRUE(embedded_test_server()->Start());
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/english_page.html"));
    // Vive la France!
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/french_page.html"));
  }

  void ValidateBrowserAllocations(base::Value* dump_json) {
    SCOPED_TRACE("Validating Browser Allocations");
    base::Value* heaps_v2 =
        FindHeapsV2(base::Process::Current().Pid(), dump_json);

    if (GetParam() == switches::kMemlogModeAll ||
        GetParam() == switches::kMemlogModeBrowser ||
        GetParam() == switches::kMemlogModeMinimal) {
      ASSERT_TRUE(heaps_v2);
      ASSERT_NO_FATAL_FAILURE(
          ValidateDump(heaps_v2, kBrowserAllocSize * kBrowserAllocCount,
                       kBrowserAllocCount, "malloc", nullptr));
      ASSERT_NO_FATAL_FAILURE(
          ValidateDump(heaps_v2, total_variadic_allocations_,
                       kBrowserAllocCount, "malloc", nullptr));
      ASSERT_NO_FATAL_FAILURE(ValidateDump(
          heaps_v2, kPartitionAllocSize * kPartitionAllocCount,
          kPartitionAllocCount, "partition_alloc", kPartitionAllocTypeName));
    } else {
      ASSERT_FALSE(heaps_v2) << "There should be no heap dump for the browser.";
    }

    EXPECT_EQ(1, NumProcessesWithName(dump_json, "Browser"));
  }

  void ValidateRendererAllocations(base::Value* dump_json) {
    SCOPED_TRACE("Validating Renderer Allocation");
    base::ProcessId renderer_pid = base::GetProcId(browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetMainFrame()
                                                       ->GetProcess()
                                                       ->GetHandle());
    base::Value* heaps_v2 = FindHeapsV2(renderer_pid, dump_json);
    if (GetParam() == switches::kMemlogModeAll ||
        GetParam() == switches::kMemlogModeRendererSampling) {
      ASSERT_TRUE(heaps_v2);

      // ValidateDump doesn't always succeed for the renderer, since we don't do
      // anything to flush allocations, there are very few allocations recorded
      // by the heap profiler. When we do a heap dump, we prune small
      // allocations...and this can cause all allocations to be pruned.
      // ASSERT_NO_FATAL_FAILURE(ValidateDump(dump_json.get(), 0, 0));
    } else {
      ASSERT_FALSE(heaps_v2)
          << "There should be no heap dump for the renderer.";
    }

    // RendererSampling guarantees only 1 renderer is ever sampled at a time.
    if (GetParam() == switches::kMemlogModeRendererSampling) {
      EXPECT_EQ(1, NumProcessesWithName(dump_json, "Renderer"));
    } else {
      EXPECT_GT(NumProcessesWithName(dump_json, "Renderer"), 0);
    }
  }

 private:
  std::vector<char*> leaks_;
  size_t total_variadic_allocations_ = 0;
  base::PartitionAllocatorGeneric partition_allocator_;
};

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
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Do a dummy dump of the browser process as a way to synchronize the test
  // on profiling process start.
  DumpProcess(
      base::Process::Current().Pid(),
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("throwaway.json.gz")));

  MakeTestAllocations();

  // Attempt to dump a browser process.
  {
    base::FilePath browser_dumpfile_path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("browserdump.json.gz"));
    DumpProcess(base::Process::Current().Pid(), browser_dumpfile_path);

    std::unique_ptr<base::Value> dump_json =
        ReadDumpFile(browser_dumpfile_path);
    if (GetParam() == switches::kMemlogModeAll ||
        GetParam() == switches::kMemlogModeBrowser ||
        GetParam() == switches::kMemlogModeMinimal) {
      ASSERT_TRUE(dump_json);
      EXPECT_EQ(0, NumProcessesWithName(dump_json.get(), "Renderer"));
      ValidateBrowserAllocations(dump_json.get());
    } else {
      ASSERT_FALSE(dump_json) << "Browser process unexpectedly profiled.";
    }
  }

  // Attempt to dump a renderer process.
  {
    base::ProcessId renderer_pid = base::GetProcId(browser()
                                                       ->tab_strip_model()
                                                       ->GetActiveWebContents()
                                                       ->GetMainFrame()
                                                       ->GetProcess()
                                                       ->GetHandle());
    base::FilePath renderer_dumpfile_path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("rendererdump.json.gz"));
    DumpProcess(renderer_pid, renderer_dumpfile_path);
    std::unique_ptr<base::Value> dump_json =
        ReadDumpFile(renderer_dumpfile_path);

    if (GetParam() == switches::kMemlogModeAll ||
        GetParam() == switches::kMemlogModeRendererSampling) {
      ASSERT_TRUE(dump_json);
      ValidateRendererAllocations(dump_json.get());
      EXPECT_EQ(0, NumProcessesWithName(dump_json.get(), "Browser"));
    } else {
      ASSERT_FALSE(dump_json) << "Renderer process unexpectedly profiled.";
    }
  }

  // Attempt to dump a gpu process.
  // TODO(ajwong): Implement this.  http://crbug.com/780955
}

// Ensure invocations via TracingController can generate a valid JSON file with
// expected data.
IN_PROC_BROWSER_TEST_P(MemlogBrowserTest, TracingControllerEndToEnd) {
  profiling::ProfilingTestDriver driver;
  profiling::ProfilingTestDriver::Options options;
  options.mode =
      GetParam()
          ? profiling::ProfilingProcessHost::ConvertStringToMode(GetParam())
          : profiling::ProfilingProcessHost::Mode::kNone;
  options.profiling_already_started = true;

  EXPECT_TRUE(driver.RunTest(options));
}

// TODO(ajwong): Test what happens if profiling process crashes.
// http://crbug.com/780955

INSTANTIATE_TEST_CASE_P(NoMemlog,
                        MemlogBrowserTest,
                        ::testing::Values(static_cast<const char*>(nullptr)));
INSTANTIATE_TEST_CASE_P(Minimal,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeMinimal));
INSTANTIATE_TEST_CASE_P(AllProcesses,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeAll));
INSTANTIATE_TEST_CASE_P(BrowserOnly,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeBrowser));
INSTANTIATE_TEST_CASE_P(GpuOnly,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeGpu));
INSTANTIATE_TEST_CASE_P(
    RendererSampling,
    MemlogBrowserTest,
    ::testing::Values(switches::kMemlogModeRendererSampling));

}  // namespace profiling

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
