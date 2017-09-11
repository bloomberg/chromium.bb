// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/features.h"
#include "base/json/json_reader.h"
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
                  int expected_alloc,
                  int expected_alloc_count,
                  int expected_flush_fill_alloc) {
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

  base::Value* sizes = heaps_v2->FindPath({"allocators", "malloc", "sizes"});
  ASSERT_TRUE(sizes);
  const base::Value::ListStorage& sizes_list = sizes->GetList();
  EXPECT_FALSE(sizes_list.empty());

  base::Value* counts = heaps_v2->FindPath({"allocators", "malloc", "counts"});
  ASSERT_TRUE(counts);
  const base::Value::ListStorage& counts_list = counts->GetList();
  EXPECT_FALSE(counts_list.empty());
  EXPECT_EQ(sizes_list.size(), counts_list.size());

  // If given an expected allocation to look for, search the sizes and counts
  // list for them.
  if (expected_alloc) {
    bool found_browser_alloc = false;
    bool found_browser_fill_alloc = false;
    size_t browser_alloc_index = 0;
    for (size_t i = 0; i < sizes_list.size(); i++) {
      if (sizes_list[i].GetInt() == expected_alloc) {
        browser_alloc_index = i;
        found_browser_alloc = true;
      } else if (sizes_list[i].GetInt() == expected_flush_fill_alloc) {
        found_browser_fill_alloc = true;
      }
    }

    ASSERT_TRUE(found_browser_fill_alloc)
        << "Fill alloc not found. Did the send buffer not flush?";
    ASSERT_TRUE(found_browser_alloc);

    // This could be EXPECT_EQ, but that's not robust to others making allocs of
    // the given size.
    EXPECT_GE(counts->GetList()[browser_alloc_index].GetInt(),
              expected_alloc_count);
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
//   3) Flush the allocations [with more allocations]
//   4) Performs a heap dump.
//   5) Checks that the allocations from (2) made it into (4).
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
  // tested there is the existance of information.
  //
  // For the Browser allocations, because the data sending is buffered, it is
  // necessary to generate a large number of allocations to flush the buffer.
  // This test kludges the flushing by allocating two difference sizes. The
  // presence of both sizes is a good indicator that the allocations are making
  // it through.
  //
  // Intentionally leak the allocations so that they can be observed in the
  // dump.
  char* leak = nullptr;
  constexpr int kBrowserAllocSize = 103 * 1024;
  constexpr int kBrowserAllocCount = 2048;
  constexpr int kBrowserAllocFlushFillSize = 107 * 1024;
  constexpr int kBrowserAllocFlushFillCount = 2048;
  for (int i = 0; i < kBrowserAllocCount; ++i) {
    leak = new char[kBrowserAllocSize];
  }
  for (int i = 0; i < kBrowserAllocFlushFillCount; ++i) {
    leak = new char[kBrowserAllocFlushFillSize];
  }

  // Assuming an average stack size of 20 frames, each alloc packet is ~160
  // bytes in size, and there are 400 packets to fill up a channel. There are 17
  // channels, so ~6800 allocations should flush all channels. But this assumes
  // even distribution of allocations across channels. Unfortunately, we're
  // using a fairly dumb hash function. To prevent test flakiness, increase
  // those allocations by an order of magnitude.
  for (int i = 0; i < 68000; ++i) {
    leak = new char[1];
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
    // TODO(ajwong): Make these verify allocation amounts in the browser using
    // kBrowserAllocSize, kBrowserAllocCount, and kBrowserAllocFlushFillSize
    // after the baseline tests are determined to be stable.
    ASSERT_NO_FATAL_FAILURE(ValidateDump(dump_json.get(), 0, 0, 0));
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
      // ASSERT_NO_FATAL_FAILURE(ValidateDump(dump_json.get(), 0, 0, 0));
    } else {
      ASSERT_FALSE(dump_json)
          << "Renderer should not be dumpable unless kMemlogModeAll!";
    }
  }
}

// TODO(ajwong): Test what happens if profiling proccess crashes.
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
