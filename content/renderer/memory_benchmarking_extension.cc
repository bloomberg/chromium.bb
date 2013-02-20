// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_benchmarking_extension.h"

#include "base/string_util.h"
#include "content/public/renderer/render_thread.h"

#if !defined(NO_TCMALLOC) && defined(OS_LINUX)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif  // !defined(NO_TCMALLOC) && defined(OS_LINUX)

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
        "chrome.memoryBenchmarking.heapProfilerDump = function(reason) {"
        "  native function HeapProfilerDump();"
        "  return HeapProfilerDump(reason);"
        "};"
        ) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("IsHeapProfilerRunning")))
      return v8::FunctionTemplate::New(IsHeapProfilerRunning);
    else if (name->Equals(v8::String::New("HeapProfilerDump")))
      return v8::FunctionTemplate::New(HeapProfilerDump);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> IsHeapProfilerRunning(
      const v8::Arguments& args) {
#if !defined(NO_TCMALLOC) && defined(OS_LINUX)
    return v8::Boolean::New(::IsHeapProfilerRunning());
#else
    return v8::Boolean::New(false);
#endif  // !defined(NO_TCMALLOC) && defined(OS_LINUX)
  }

  static v8::Handle<v8::Value> HeapProfilerDump(const v8::Arguments& args) {
#if !defined(NO_TCMALLOC) && defined(OS_LINUX)
    char dumped_filename_buffer[1000];
    std::string reason("benchmarking_extension");
    if (args.Length() && args[0]->IsString())
      reason = *v8::String::AsciiValue(args[0]);
    ::HeapProfilerDumpWithFileName(reason.c_str(),
                                   dumped_filename_buffer,
                                   sizeof(dumped_filename_buffer));
    return v8::String::New(dumped_filename_buffer);
#endif  // !defined(NO_TCMALLOC) && defined(OS_LINUX)
    return v8::Undefined();
  }
};

}  // namespace

namespace content {

v8::Extension* MemoryBenchmarkingExtension::Get() {
  return new MemoryBenchmarkingWrapper();
}

}  // namespace content
