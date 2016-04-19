// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/common/chrome_content_client.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/common/process_type.h"
#include "url/gurl.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using tracked_objects::BirthOnThreadSnapshot;
using tracked_objects::DeathDataSnapshot;
using tracked_objects::LocationSnapshot;
using tracked_objects::TaskSnapshot;
using tracked_objects::ProcessDataPhaseSnapshot;

namespace {

// Re-serializes the |location| into |dictionary|.
void LocationSnapshotToValue(const LocationSnapshot& location,
                             base::DictionaryValue* dictionary) {
  dictionary->SetString("file_name", location.file_name);
  // Note: This function name is not escaped, and templates have less-than
  // characters, which means this is not suitable for display as HTML unless
  // properly escaped.
  dictionary->SetString("function_name", location.function_name);
  dictionary->SetInteger("line_number", location.line_number);
}

// Re-serializes the |birth| into |dictionary|.  Prepends the |prefix| to the
// "thread" and "location" key names in the dictionary.
void BirthOnThreadSnapshotToValue(const BirthOnThreadSnapshot& birth,
                                  const std::string& prefix,
                                  base::DictionaryValue* dictionary) {
  DCHECK(!prefix.empty());

  std::unique_ptr<base::DictionaryValue> location_value(
      new base::DictionaryValue);
  LocationSnapshotToValue(birth.location, location_value.get());
  dictionary->Set(prefix + "_location", location_value.release());

  dictionary->Set(prefix + "_thread", new base::StringValue(birth.thread_name));
}

// Re-serializes the |death_data| into |dictionary|.
void DeathDataSnapshotToValue(const DeathDataSnapshot& death_data,
                              base::DictionaryValue* dictionary) {
  dictionary->SetInteger("count", death_data.count);
  dictionary->SetInteger("run_ms", death_data.run_duration_sum);
  dictionary->SetInteger("run_ms_max", death_data.run_duration_max);
  dictionary->SetInteger("run_ms_sample", death_data.run_duration_sample);
  dictionary->SetInteger("queue_ms", death_data.queue_duration_sum);
  dictionary->SetInteger("queue_ms_max", death_data.queue_duration_max);
  dictionary->SetInteger("queue_ms_sample", death_data.queue_duration_sample);
}

// Re-serializes the |snapshot| into |dictionary|.
void TaskSnapshotToValue(const TaskSnapshot& snapshot,
                         base::DictionaryValue* dictionary) {
  BirthOnThreadSnapshotToValue(snapshot.birth, "birth", dictionary);

  std::unique_ptr<base::DictionaryValue> death_data(new base::DictionaryValue);
  DeathDataSnapshotToValue(snapshot.death_data, death_data.get());
  dictionary->Set("death_data", death_data.release());

  dictionary->SetString("death_thread", snapshot.death_thread_name);
}

int AsChromeProcessType(
    metrics::ProfilerEventProto::TrackedObject::ProcessType process_type) {
  switch (process_type) {
    case metrics::ProfilerEventProto::TrackedObject::UNKNOWN:
    case metrics::ProfilerEventProto::TrackedObject::PLUGIN:
      return content::PROCESS_TYPE_UNKNOWN;
    case metrics::ProfilerEventProto::TrackedObject::BROWSER:
      return content::PROCESS_TYPE_BROWSER;
    case metrics::ProfilerEventProto::TrackedObject::RENDERER:
      return content::PROCESS_TYPE_RENDERER;
    case metrics::ProfilerEventProto::TrackedObject::WORKER:
      return content::PROCESS_TYPE_WORKER_DEPRECATED;
    case metrics::ProfilerEventProto::TrackedObject::NACL_LOADER:
      return PROCESS_TYPE_NACL_LOADER;
    case metrics::ProfilerEventProto::TrackedObject::UTILITY:
      return content::PROCESS_TYPE_UTILITY;
    case metrics::ProfilerEventProto::TrackedObject::PROFILE_IMPORT:
      return content::PROCESS_TYPE_UNKNOWN;
    case metrics::ProfilerEventProto::TrackedObject::ZYGOTE:
      return content::PROCESS_TYPE_ZYGOTE;
    case metrics::ProfilerEventProto::TrackedObject::SANDBOX_HELPER:
      return content::PROCESS_TYPE_SANDBOX_HELPER;
    case metrics::ProfilerEventProto::TrackedObject::NACL_BROKER:
      return PROCESS_TYPE_NACL_BROKER;
    case metrics::ProfilerEventProto::TrackedObject::GPU:
      return content::PROCESS_TYPE_GPU;
    case metrics::ProfilerEventProto::TrackedObject::PPAPI_PLUGIN:
      return content::PROCESS_TYPE_PPAPI_PLUGIN;
    case metrics::ProfilerEventProto::TrackedObject::PPAPI_BROKER:
      return content::PROCESS_TYPE_PPAPI_BROKER;
  }
  NOTREACHED();
  return content::PROCESS_TYPE_UNKNOWN;
}

}  // anonymous namespace

namespace task_profiler {

// static
void TaskProfilerDataSerializer::ToValue(
    const ProcessDataPhaseSnapshot& process_data_phase,
    base::ProcessId process_id,
    metrics::ProfilerEventProto::TrackedObject::ProcessType process_type,
    base::DictionaryValue* dictionary) {
  std::unique_ptr<base::ListValue> tasks_list(new base::ListValue);
  for (const auto& task : process_data_phase.tasks) {
    std::unique_ptr<base::DictionaryValue> snapshot(new base::DictionaryValue);
    TaskSnapshotToValue(task, snapshot.get());
    tasks_list->Append(snapshot.release());
  }
  dictionary->Set("list", tasks_list.release());

  dictionary->SetInteger("process_id", process_id);
  dictionary->SetString("process_type", content::GetProcessTypeNameInEnglish(
                                            AsChromeProcessType(process_type)));
}

}  // namespace task_profiler
