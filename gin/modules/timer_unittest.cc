// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/timer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {

namespace {

class Result : public Wrappable<Result> {
 public:
  static WrapperInfo kWrapperInfo;
  static Handle<Result> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new Result());
  }

  int count() const { return count_; }
  void set_count(int count) { count_ = count; }

  void Quit() { base::RunLoop::QuitCurrentDeprecated(); }

 private:
  Result() : count_(0) {
  }

  ~Result() override {}

  ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<Result>::GetObjectTemplateBuilder(isolate)
        .SetProperty("count", &Result::count, &Result::set_count)
        .SetMethod("quit", &Result::Quit);
  }

  int count_;
};

WrapperInfo Result::kWrapperInfo = { gin::kEmbedderNativeGin };

struct TestHelper {
  TestHelper(v8::Isolate* isolate)
      : runner(new ShellRunner(&delegate, isolate)),
        scope(runner.get()),
        timer_module(TimerModule::Create(isolate)),
        result(Result::Create(isolate)) {
    EXPECT_FALSE(runner->global().IsEmpty());
    runner->global()->Set(StringToV8(isolate, "timer"),
                          timer_module->GetWrapper(isolate).ToLocalChecked());
    runner->global()->Set(StringToV8(isolate, "result"),
                          result->GetWrapper(isolate).ToLocalChecked());
  }

  ShellRunnerDelegate delegate;
  std::unique_ptr<ShellRunner> runner;
  Runner::Scope scope;
  Handle<TimerModule> timer_module;
  Handle<Result> result;
};

}  // namespace

typedef V8Test TimerUnittest;

TEST_F(TimerUnittest, OneShot) {
  TestHelper helper(instance_->isolate());
  std::string source =
     "timer.createOneShot(0, function() {"
     "  result.count++;"
     "});";

  helper.runner->Run(source, "script");
  EXPECT_EQ(0, helper.result->count());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, helper.result->count());
}

TEST_F(TimerUnittest, OneShotCancel) {
  TestHelper helper(instance_->isolate());
  std::string source =
     "var t = timer.createOneShot(0, function() {"
     "  result.count++;"
     "});"
     "t.cancel()";

  helper.runner->Run(source, "script");
  EXPECT_EQ(0, helper.result->count());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, helper.result->count());
}

TEST_F(TimerUnittest, Repeating) {
  TestHelper helper(instance_->isolate());

  // TODO(aa): Cannot do: if (++result.count == 3) because of v8 bug. Create
  // test case and report.
  std::string source =
      "var t = timer.createRepeating(0, function() {"
      "  result.count++;"
      "  if (result.count == 3) {"
      "    /* Cancel the timer to prevent a hang when ScopedTaskEnvironment "
      "       flushes main thread tasks. */"
      "    t.cancel();"
      "    result.quit();"
      "  }"
      "});";

  helper.runner->Run(source, "script");
  EXPECT_EQ(0, helper.result->count());

  base::RunLoop().Run();
  EXPECT_EQ(3, helper.result->count());
}

TEST_F(TimerUnittest, TimerCallbackToDestroyedRunner) {
  TestHelper helper(instance_->isolate());
  std::string source =
     "timer.createOneShot(0, function() {"
     "  result.count++;"
     "});";

  helper.runner->Run(source, "script");
  EXPECT_EQ(0, helper.result->count());

  // Destroy runner, which should destroy the timer object we created.
  helper.runner.reset(NULL);
  base::RunLoop().RunUntilIdle();

  // Timer should not have run because it was deleted.
  EXPECT_EQ(0, helper.result->count());
}

}  // namespace gin
