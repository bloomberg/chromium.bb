// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/file_runner.h"

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "gin/converter.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/gtest.h"
#include "gin/try_catch.h"
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
}

FileRunnerDelegate::~FileRunnerDelegate() {
}

void FileRunnerDelegate::UnhandledException(Runner* runner,
                                            TryCatch& try_catch) {
  ModuleRunnerDelegate::UnhandledException(runner, try_catch);
  FAIL() << try_catch.GetStackTrace();
}

void RunTestFromFile(const base::FilePath& path, FileRunnerDelegate* delegate) {
  ASSERT_TRUE(base::PathExists(path)) << path.LossyDisplayName();
  std::string source;
  ASSERT_TRUE(ReadFileToString(path, &source));

  base::MessageLoop message_loop;

  gin::IsolateHolder instance;
  gin::Runner runner(delegate, instance.isolate());
  {
    gin::Runner::Scope scope(&runner);
    v8::V8::SetCaptureStackTraceForUncaughtExceptions(true);
    runner.Run(source, path.AsUTF8Unsafe());

    message_loop.RunUntilIdle();

    v8::Handle<v8::Value> result = runner.context()->Global()->Get(
        StringToSymbol(runner.isolate(), "result"));
    EXPECT_EQ("PASS", V8ToString(result));
  }
}

}  // namespace gin
