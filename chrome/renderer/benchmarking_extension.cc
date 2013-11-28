// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/benchmarking_extension.h"

#include "base/command_line.h"
#include "base/metrics/stats_table.h"
#include "base/time/time.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "v8/include/v8.h"

const char kBenchmarkingExtensionName[] = "v8/Benchmarking";

namespace extensions_v8 {

class BenchmarkingWrapper : public v8::Extension {
 public:
  BenchmarkingWrapper() :
      v8::Extension(kBenchmarkingExtensionName,
        "if (typeof(chrome) == 'undefined') {"
        "  chrome = {};"
        "};"
        "if (typeof(chrome.benchmarking) == 'undefined') {"
        "  chrome.benchmarking = {};"
        "};"
        "chrome.benchmarking.counter = function(name) {"
        "  native function GetCounter();"
        "  return GetCounter(name);"
        "};"
        "chrome.benchmarking.counterForRenderer = function(name) {"
        "  native function GetCounterForRenderer();"
        "  return GetCounterForRenderer(name);"
        "};"
        "chrome.benchmarking.isSingleProcess = function() {"
        "  native function IsSingleProcess();"
        "  return IsSingleProcess();"
        "};"
        "chrome.Interval = function() {"
        "  var start_ = 0;"
        "  var stop_ = 0;"
        "  native function HiResTime();"
        "  this.start = function() {"
        "    stop_ = 0;"
        "    start_ = HiResTime();"
        "  };"
        "  this.stop = function() {"
        "    stop_ = HiResTime();"
        "    if (start_ == 0)"
        "      stop_ = 0;"
        "  };"
        "  this.microseconds = function() {"
        "    var stop = stop_;"
        "    if (stop == 0 && start_ != 0)"
        "      stop = HiResTime();"
        "    return Math.ceil(stop - start_);"
        "  };"
        "}"
        ) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetCounter"))) {
      return v8::FunctionTemplate::New(isolate, GetCounter);
    } else if (name->Equals(
                   v8::String::NewFromUtf8(isolate, "GetCounterForRenderer"))) {
      return v8::FunctionTemplate::New(isolate, GetCounterForRenderer);
    } else if (name->Equals(
                   v8::String::NewFromUtf8(isolate, "IsSingleProcess"))) {
      return v8::FunctionTemplate::New(isolate, IsSingleProcess);
    } else if (name->Equals(v8::String::NewFromUtf8(isolate, "HiResTime"))) {
      return v8::FunctionTemplate::New(isolate, HiResTime);
    }

    return v8::Handle<v8::FunctionTemplate>();
  }

  /*
   * Extract the counter name from arguments.
   */
  static void ExtractCounterName(
      const v8::FunctionCallbackInfo<v8::Value>& args,
      char* name,
      size_t capacity) {
    name[0] = 'c';
    name[1] = ':';
    args[0]->ToString()->WriteUtf8(&name[2], capacity - 3);
  }

  static void GetCounter(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (!args.Length() || !args[0]->IsString() || !base::StatsTable::current())
      return;

    char name[256];
    ExtractCounterName(args, name, sizeof(name));
    int counter = base::StatsTable::current()->GetCounterValue(name);
    args.GetReturnValue().Set(static_cast<int32_t>(counter));
  }

  static void GetCounterForRenderer(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (!args.Length() || !args[0]->IsString() || !base::StatsTable::current())
      return;

    char name[256];
    ExtractCounterName(args, name, sizeof(name));
    int counter = base::StatsTable::current()->GetCounterValue(
        name,
        base::GetCurrentProcId());
    args.GetReturnValue().Set(static_cast<int32_t>(counter));
  }

  static void IsSingleProcess(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(
       CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  }

  static void HiResTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(
        static_cast<double>(base::TimeTicks::HighResNow().ToInternalValue()));
  }
};

v8::Extension* BenchmarkingExtension::Get() {
  return new BenchmarkingWrapper();
}

}  // namespace extensions_v8
