// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/file_runner.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/file.h"
#include "gin/test/gc.h"
#include "gin/test/gtest.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

namespace {

std::vector<base::FilePath> GetModuleSearchPaths() {
  std::vector<base::FilePath> search_paths(2);
  PathService::Get(base::DIR_SOURCE_ROOT, &search_paths[0]);
  PathService::Get(base::DIR_EXE, &search_paths[1]);
  search_paths[1] = search_paths[1].AppendASCII("gen");
  return search_paths;
}

}  // namespace

FileRunnerDelegate::FileRunnerDelegate()
    : ModuleRunnerDelegate(GetModuleSearchPaths()) {
  AddBuiltinModule(Console::kModuleName, Console::GetModule);
  AddBuiltinModule(GTest::kModuleName, GTest::GetModule);
  AddBuiltinModule(GC::kModuleName, GC::GetModule);
  AddBuiltinModule(File::kModuleName, File::GetModule);
}

FileRunnerDelegate::~FileRunnerDelegate() = default;

void FileRunnerDelegate::UnhandledException(ShellRunner* runner,
                                            TryCatch& try_catch) {
  ModuleRunnerDelegate::UnhandledException(runner, try_catch);
  FAIL() << try_catch.GetStackTrace();
}

void RunTestFromFile(const base::FilePath& path, FileRunnerDelegate* delegate,
                     bool run_until_idle) {
  ASSERT_TRUE(base::PathExists(path)) << path.LossyDisplayName();
  std::string source;
  ASSERT_TRUE(ReadFileToString(path, &source));

  base::test::ScopedTaskEnvironment scoped_task_environment;

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::IsolateHolder::kStableV8Extras,
                                 gin::ArrayBufferAllocator::SharedInstance());

  gin::IsolateHolder instance(base::ThreadTaskRunnerHandle::Get());
  gin::ShellRunner runner(delegate, instance.isolate());
  {
    gin::Runner::Scope scope(&runner);
    instance.isolate()->SetCaptureStackTraceForUncaughtExceptions(true);
    runner.Run(source, path.AsUTF8Unsafe());

    if (run_until_idle) {
      base::RunLoop().RunUntilIdle();
    } else {
      base::RunLoop().Run();
    }

    v8::Local<v8::Value> result = runner.global()->Get(
        StringToSymbol(runner.GetContextHolder()->isolate(), "result"));
    EXPECT_EQ("PASS", V8ToString(result));
  }
}

}  // namespace gin
