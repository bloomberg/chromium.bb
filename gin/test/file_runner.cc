// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/file_runner.h"

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "gin/converter.h"
#include "gin/modules/module_registry.h"
#include "gin/test/gtest.h"
#include "gin/try_catch.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

namespace {

base::FilePath GetModuleBase() {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path;
}

}  // namespace

FileRunnerDelegate::FileRunnerDelegate()
    : ModuleRunnerDelegate(GetModuleBase()) {
}

FileRunnerDelegate::~FileRunnerDelegate() {
}

void FileRunnerDelegate::DidCreateContext(Runner* runner) {
  ModuleRunnerDelegate::DidCreateContext(runner);

  v8::Handle<v8::Context> context = runner->context();
  ModuleRegistry* registry = ModuleRegistry::From(context);

  registry->AddBuiltinModule(runner->isolate(), "gtest",
                             GetGTestTemplate(runner->isolate()));
}

void FileRunnerDelegate::UnhandledException(Runner* runner,
                                            TryCatch& try_catch) {
  ModuleRunnerDelegate::UnhandledException(runner, try_catch);
  EXPECT_FALSE(try_catch.HasCaught()) << try_catch.GetPrettyMessage();
}

void RunTestFromFile(const base::FilePath& path, RunnerDelegate* delegate) {
  ASSERT_TRUE(base::PathExists(path)) << path.LossyDisplayName();
  std::string source;
  ASSERT_TRUE(ReadFileToString(path, &source));

  base::MessageLoop message_loop;

  gin::Runner runner(delegate, v8::Isolate::GetCurrent());
  gin::Runner::Scope scope(&runner);
  runner.Run(source);

  message_loop.RunUntilIdle();

  v8::Handle<v8::Value> result = runner.context()->Global()->Get(
      StringToSymbol(runner.isolate(), "result"));
  std::string result_string;
  ASSERT_TRUE(ConvertFromV8(result, &result_string));
  EXPECT_EQ("PASS", result_string);
}

}  // namespace gin
