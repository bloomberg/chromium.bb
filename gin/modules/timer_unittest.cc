// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/timer.h"

#include "base/message_loop/message_loop.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/runner.h"
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

  void quit() {
    base::MessageLoop::current()->Quit();
  }

 private:
  Result() : count_(0) {
  }

  virtual ~Result() {
  }

  virtual ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE {
    return Wrappable<Result>::GetObjectTemplateBuilder(isolate)
        .SetProperty("count", &Result::count, &Result::set_count)
        .SetMethod("quit", &Result::quit);
  }

  int count_;
};

WrapperInfo Result::kWrapperInfo = { gin::kEmbedderNativeGin };

}  // namespace

typedef V8Test TimerUnittest;

TEST_F(TimerUnittest, OneShot) {
  v8::Isolate* isolate = instance_->isolate();

  RunnerDelegate delegate;
  Runner runner(&delegate, isolate);
  Runner::Scope scope(&runner);

  Handle<TimerModule> timer_module = TimerModule::Create(isolate);
  Handle<Result> result = Result::Create(isolate);

  EXPECT_FALSE(runner.global().IsEmpty());
  runner.global()->Set(StringToV8(isolate, "timer"),
                       timer_module->GetWrapper(isolate));
  runner.global()->Set(StringToV8(isolate, "result"),
                       result->GetWrapper(isolate));

  std::string source =
     "timer.createOneShot(100, function() {"
     "  result.count++;"
     "  result.quit();"
     "});";

  base::MessageLoop loop(base::MessageLoop::TYPE_DEFAULT);
  runner.Run(source, "script");
  EXPECT_EQ(0, result->count());

  loop.Run();
  loop.RunUntilIdle();

  EXPECT_EQ(1, result->count());
}

TEST_F(TimerUnittest, Repeating) {
  v8::Isolate* isolate = instance_->isolate();

  RunnerDelegate delegate;
  Runner runner(&delegate, isolate);
  Runner::Scope scope(&runner);

  Handle<TimerModule> timer_module = TimerModule::Create(isolate);
  Handle<Result> result = Result::Create(isolate);

  EXPECT_FALSE(runner.global().IsEmpty());
  runner.global()->Set(StringToV8(isolate, "timer"),
                       timer_module->GetWrapper(isolate));
  runner.global()->Set(StringToV8(isolate, "result"),
                       result->GetWrapper(isolate));

  // TODO(aa): Cannot do: if (++result.count == 3) because of v8 bug. Create
  // test case and report.
  std::string source =
     "timer.createRepeating(10, function() {"
     "  result.count++;"
     "  if (result.count == 3) {"
     "    result.quit();"
     "  }"
     "});";

  base::MessageLoop loop(base::MessageLoop::TYPE_DEFAULT);
  runner.Run(source, "script");
  EXPECT_EQ(0, result->count());

  loop.Run();
  EXPECT_EQ(3, result->count());
}

}  // namespace gin
