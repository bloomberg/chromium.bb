// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include "v8/include/v8.h"

const char kGpuBenchmarkingExtensionName[] = "v8/GpuBenchmarking";

namespace content {

class GpuBenchmarkingWrapper : public v8::Extension {
 public:
  GpuBenchmarkingWrapper() :
      v8::Extension(kGpuBenchmarkingExtensionName,
          "if (typeof(chrome) == 'undefined') {"
          "  chrome = {};"
          "};"
          "if (typeof(chrome.gpuBenchmarking) == 'undefined') {"
          "  chrome.gpuBenchmarking = {};"
          "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    return v8::Handle<v8::FunctionTemplate>();
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
