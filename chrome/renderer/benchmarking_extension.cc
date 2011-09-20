// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/benchmarking_extension.h"

#include "base/command_line.h"
#include "base/metrics/stats_table.h"
#include "base/time.h"
#include "chrome/common/benchmarking_messages.h"
#include "content/common/child_thread.h"
#include "content/common/content_switches.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "v8/include/v8.h"

using WebKit::WebCache;

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
        "chrome.benchmarking.clearCache = function(preserve_ssl_entries) {"
        "  native function ClearCache();"
        "  ClearCache(preserve_ssl_entries);"
        "};"
        "chrome.benchmarking.clearHostResolverCache = function() {"
        "  native function ClearHostResolverCache();"
        "  ClearHostResolverCache();"
        "};"
        "chrome.benchmarking.clearPredictorCache = function() {"
        "  native function ClearPredictorCache();"
        "  ClearPredictorCache();"
        "};"
        "chrome.benchmarking.closeConnections = function() {"
        "  native function CloseConnections();"
        "  CloseConnections();"
        "};"
        "chrome.benchmarking.counter = function(name) {"
        "  native function GetCounter();"
        "  return GetCounter(name);"
        "};"
        "chrome.benchmarking.enableSpdy = function(name) {"
        "  native function EnableSpdy();"
        "  EnableSpdy(name);"
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
    if (name->Equals(v8::String::New("CloseConnections"))) {
      return v8::FunctionTemplate::New(CloseConnections);
    } else if (name->Equals(v8::String::New("ClearCache"))) {
      return v8::FunctionTemplate::New(ClearCache);
    } else if (name->Equals(v8::String::New("ClearHostResolverCache"))) {
      return v8::FunctionTemplate::New(ClearHostResolverCache);
    } else if (name->Equals(v8::String::New("ClearPredictorCache"))) {
      return v8::FunctionTemplate::New(ClearPredictorCache);
    } else if (name->Equals(v8::String::New("EnableSpdy"))) {
      return v8::FunctionTemplate::New(EnableSpdy);
    } else if (name->Equals(v8::String::New("GetCounter"))) {
      return v8::FunctionTemplate::New(GetCounter);
    } else if (name->Equals(v8::String::New("IsSingleProcess"))) {
      return v8::FunctionTemplate::New(IsSingleProcess);
    } else if (name->Equals(v8::String::New("HiResTime"))) {
      return v8::FunctionTemplate::New(HiResTime);
    }

    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> CloseConnections(const v8::Arguments& args) {
    ChildThread::current()->Send(
        new ChromeViewHostMsg_CloseCurrentConnections());
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ClearCache(const v8::Arguments& args) {
    bool preserve_ssl_host_entries = false;
    if (args.Length() && args[0]->IsBoolean())
      preserve_ssl_host_entries = args[0]->BooleanValue();
    int rv;
    ChildThread::current()->Send(new ChromeViewHostMsg_ClearCache(
        preserve_ssl_host_entries, &rv));
    WebCache::clear();
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ClearHostResolverCache(
      const v8::Arguments& args) {
    int rv;
    ChildThread::current()->Send(
        new ChromeViewHostMsg_ClearHostResolverCache(&rv));
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ClearPredictorCache(
      const v8::Arguments& args) {
    int rv;
    ChildThread::current()->Send(new ChromeViewHostMsg_ClearPredictorCache(
        &rv));
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> EnableSpdy(const v8::Arguments& args) {
    if (!args.Length() || !args[0]->IsBoolean())
      return v8::Undefined();

    ChildThread::current()->Send(new ChromeViewHostMsg_EnableSpdy(
        args[0]->BooleanValue()));
    return v8::Undefined();
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
