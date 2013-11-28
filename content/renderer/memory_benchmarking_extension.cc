// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_benchmarking_extension.h"

#include "base/strings/string_util.h"
#include "content/common/memory_benchmark_messages.h"
#include "content/renderer/render_thread_impl.h"

#if defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))

#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"

namespace {

const char kMemoryBenchmarkingExtensionName[] = "v8/MemoryBenchmarking";

}

namespace {

class MemoryBenchmarkingWrapper : public v8::Extension {
 public:
  MemoryBenchmarkingWrapper() :
      v8::Extension(kMemoryBenchmarkingExtensionName,
        "if (typeof(chrome) == 'undefined') {"
        "  chrome = {};"
        "};"
        "if (typeof(chrome.memoryBenchmarking) == 'undefined') {"
        "  chrome.memoryBenchmarking = {};"
        "};"
        "chrome.memoryBenchmarking.isHeapProfilerRunning = function() {"
        "  native function IsHeapProfilerRunning();"
        "  return IsHeapProfilerRunning();"
        "};"
        "chrome.memoryBenchmarking.heapProfilerDump = "
        "      function(process_type, reason) {"
        "  native function HeapProfilerDump();"
        "  HeapProfilerDump(process_type, reason);"
        "};"
        ) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::NewFromUtf8(isolate, "IsHeapProfilerRunning")))
      return v8::FunctionTemplate::New(isolate, IsHeapProfilerRunning);
    else if (name->Equals(v8::String::NewFromUtf8(isolate, "HeapProfilerDump")))
      return v8::FunctionTemplate::New(isolate, HeapProfilerDump);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void IsHeapProfilerRunning(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(::IsHeapProfilerRunning());
  }

  static void HeapProfilerDump(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    std::string process_type;
    if (args.Length() && args[0]->IsString())
      process_type = *v8::String::Utf8Value(args[0]);
    std::string reason("benchmarking_extension");
    if (args.Length() > 1 && args[1]->IsString())
      reason = *v8::String::Utf8Value(args[1]);
    if (process_type == "browser") {
      content::RenderThreadImpl::current()->Send(
          new MemoryBenchmarkHostMsg_HeapProfilerDump(reason));
    } else {
      ::HeapProfilerDump(reason.c_str());
    }
  }
};

}  // namespace

namespace content {

v8::Extension* MemoryBenchmarkingExtension::Get() {
  return new MemoryBenchmarkingWrapper();
}

}  // namespace content

#endif  // defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))
