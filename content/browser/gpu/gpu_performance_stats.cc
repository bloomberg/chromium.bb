// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_performance_stats.h"

#include "base/stringprintf.h"

namespace {
DictionaryValue* NewDescriptionValuePairFromFloat(const std::string& desc,
    float value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", base::StringPrintf("%.1f", value));
  return dict;
}
}  // namespace

#if !defined(OS_WIN)
// No data for other operating systems yet, just return 0.0s.
GpuPerformanceStats GpuPerformanceStats::RetrieveGpuPerformanceStats() {
  return GpuPerformanceStats();
}
#endif

base::Value* GpuPerformanceStats::ToValue() const {
  ListValue* result = new ListValue();
  result->Append(NewDescriptionValuePairFromFloat("Graphics", graphics));
  result->Append(NewDescriptionValuePairFromFloat("Gaming", gaming));
  result->Append(NewDescriptionValuePairFromFloat("Overall", overall));
  return result;
}
