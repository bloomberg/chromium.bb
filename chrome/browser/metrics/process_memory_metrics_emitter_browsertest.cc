// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/message_loop/message_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

class ProcessMemoryMetricsEmitterFake : public ProcessMemoryMetricsEmitter {
 public:
  ProcessMemoryMetricsEmitterFake() {}

 private:
  ~ProcessMemoryMetricsEmitterFake() override {}

  void ReceivedMemoryDump(
      uint64_t dump_guid,
      bool success,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) override {
    EXPECT_TRUE(success);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterFake);
};

}  // namespace

class ProcessMemoryMetricsEmitterTest : public InProcessBrowserTest {
 public:
  ProcessMemoryMetricsEmitterTest() {}
  ~ProcessMemoryMetricsEmitterTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterTest);
};

IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest, FetchAndEmitMetrics) {
  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake);
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  content::RunMessageLoop();
}
