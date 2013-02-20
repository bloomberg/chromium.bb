// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/benchmarking_extension.h"

#include "base/command_line.h"
#include "base/metrics/stats_table.h"
#include "base/time.h"
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

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetCounter"))) {
      return v8::FunctionTemplate::New(GetCounter);
    } else if (name->Equals(v8::String::New("IsSingleProcess"))) {
      return v8::FunctionTemplate::New(IsSingleProcess);
    } else if (name->Equals(v8::String::New("HiResTime"))) {
      return v8::FunctionTemplate::New(HiResTime);
    }

    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> GetCounter(const v8::Arguments& args) {
    if (!args.Length() || !args[0]->IsString() || !base::StatsTable::current())
      return v8::Undefined();

    // Extract the name argument
    char name[256];
    name[0] = 'c';
    name[1] = ':';
    args[0]->ToString()->WriteAscii(&name[2], 0, sizeof(name) - 3);

    int counter = base::StatsTable::current()->GetCounterValue(name);
    return v8::Integer::New(counter);
  }

  static v8::Handle<v8::Value> IsSingleProcess(const v8::Arguments& args) {
    return v8::Boolean::New(
       CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  }

  static v8::Handle<v8::Value> HiResTime(const v8::Arguments& args) {
    return v8::Number::New(
        static_cast<double>(base::TimeTicks::HighResNow().ToInternalValue()));
  }
};

v8::Extension* BenchmarkingExtension::Get() {
  return new BenchmarkingWrapper();
}

}  // namespace extensions_v8
