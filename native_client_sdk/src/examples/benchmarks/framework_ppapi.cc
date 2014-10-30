// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ppapi/cpp/input_event.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/point.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array.h>
#include <ppapi/cpp/var_dictionary.h>
#include <ppapi/utility/completion_callback_factory.h>
#include <stdint.h>
#include <sys/time.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "framework.h"

void BenchmarkPost(Benchmark* b, double median, double range, void* data) {
  pp::VarDictionary benchmark;
  benchmark.Set("name", b->Name());
  std::ostringstream result;
  result << std::fixed << std::setw(8) << std::setprecision(5) <<
            median << " seconds";
  benchmark.Set("result", result.str());
  pp::VarDictionary message;
  message.Set("message", "benchmark_result");
  message.Set("benchmark", benchmark);
  pp::Instance* instance = static_cast<pp::Instance*>(data);
  instance->PostMessage(message);
}

// Pepper framework

class BenchmarksInstance : public pp::Instance {
 public:
  explicit BenchmarksInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this) {}

  ~BenchmarksInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
    return true;
  }

  virtual void HandleMessage(const pp::Var& var) {
    if (var.is_dictionary()) {
      pp::VarDictionary dictionary(var);
      std::string message = dictionary.Get("message").AsString();
      if (message == "run_benchmark") {
        BenchmarkSuite::Run("", BenchmarkPost, this);
        pp::VarDictionary message;
        message.Set("message", "benchmark_finish");
        PostMessage(message);
      }
    }
  }

 private:
  pp::CompletionCallbackFactory<BenchmarksInstance> callback_factory_;
};

class BenchmarksModule : public pp::Module {
 public:
  BenchmarksModule() : pp::Module() {}
  virtual ~BenchmarksModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new BenchmarksInstance(instance);
  }
};


namespace pp {
Module* CreateModule() { return new BenchmarksModule(); }
}  // namespace pp
