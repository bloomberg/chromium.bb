// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace base {
namespace trace_event {

ProcessMemoryDump::ProcessMemoryDump() {
}

ProcessMemoryDump::~ProcessMemoryDump() {
}

void ProcessMemoryDump::AppendAsTraceFormat(std::string* out) const {
  // Build up the [dumper name] -> [serialized snapshot] JSON dictionary.
  DictionaryValue dict;
  std::string json_dict;
  // TODO(primiano): this will append here the actual dumps from the dumpers.
  base::JSONWriter::Write(&dict, &json_dict);
  *out += json_dict;
}

}  // namespace trace_event
}  // namespace base
