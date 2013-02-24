// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "content/public/common/content_client.h"
#include "content/public/common/process_type.h"
#include "googleurl/src/gurl.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using tracked_objects::BirthOnThreadSnapshot;
using tracked_objects::DeathDataSnapshot;
using tracked_objects::LocationSnapshot;
using tracked_objects::ParentChildPairSnapshot;
using tracked_objects::TaskSnapshot;
using tracked_objects::ProcessDataSnapshot;

namespace {

// Re-serializes the |location| into |dictionary|.
void LocationSnapshotToValue(const LocationSnapshot& location,
                             DictionaryValue* dictionary) {
  dictionary->Set("file_name", Value::CreateStringValue(location.file_name));
  // Note: This function name is not escaped, and templates have less-than
  // characters, which means this is not suitable for display as HTML unless
  // properly escaped.
  dictionary->Set("function_name",
                  Value::CreateStringValue(location.function_name));
  dictionary->Set("line_number",
                  Value::CreateIntegerValue(location.line_number));
}

// Re-serializes the |birth| into |dictionary|.  Prepends the |prefix| to the
// "thread" and "location" key names in the dictionary.
void BirthOnThreadSnapshotToValue(const BirthOnThreadSnapshot& birth,
                                  const std::string& prefix,
                                  DictionaryValue* dictionary) {
  DCHECK(!prefix.empty());

  scoped_ptr<DictionaryValue> location_value(new DictionaryValue);
  LocationSnapshotToValue(birth.location, location_value.get());
  dictionary->Set(prefix + "_location", location_value.release());

  dictionary->Set(prefix + "_thread",
                  Value::CreateStringValue(birth.thread_name));
}

// Re-serializes the |death_data| into |dictionary|.
void DeathDataSnapshotToValue(const DeathDataSnapshot& death_data,
                              base::DictionaryValue* dictionary) {
  dictionary->Set("count",
                  Value::CreateIntegerValue(death_data.count));
  dictionary->Set("run_ms",
                  Value::CreateIntegerValue(death_data.run_duration_sum));
  dictionary->Set("run_ms_max",
                  Value::CreateIntegerValue(death_data.run_duration_max));
  dictionary->Set("run_ms_sample",
                  Value::CreateIntegerValue(death_data.run_duration_sample));
  dictionary->Set("queue_ms",
                  Value::CreateIntegerValue(death_data.queue_duration_sum));
  dictionary->Set("queue_ms_max",
                  Value::CreateIntegerValue(death_data.queue_duration_max));
  dictionary->Set("queue_ms_sample",
                  Value::CreateIntegerValue(death_data.queue_duration_sample));

}

// Re-serializes the |snapshot| into |dictionary|.
void TaskSnapshotToValue(const TaskSnapshot& snapshot,
                         base::DictionaryValue* dictionary) {
  BirthOnThreadSnapshotToValue(snapshot.birth, "birth", dictionary);

  scoped_ptr<DictionaryValue> death_data(new DictionaryValue);
  DeathDataSnapshotToValue(snapshot.death_data, death_data.get());
  dictionary->Set("death_data", death_data.release());

  dictionary->Set("death_thread",
                  Value::CreateStringValue(snapshot.death_thread_name));

}

}  // anonymous namespace

namespace task_profiler {

// static
void TaskProfilerDataSerializer::ToValue(
    const ProcessDataSnapshot& process_data,
    content::ProcessType process_type,
    base::DictionaryValue* dictionary) {
  scoped_ptr<base::ListValue> tasks_list(new base::ListValue);
  for (std::vector<TaskSnapshot>::const_iterator it =
           process_data.tasks.begin();
       it != process_data.tasks.end(); ++it) {
    scoped_ptr<DictionaryValue> snapshot(new DictionaryValue);
    TaskSnapshotToValue(*it, snapshot.get());
    tasks_list->Append(snapshot.release());
  }
  dictionary->Set("list", tasks_list.release());

  dictionary->SetInteger("process_id", process_data.process_id);
  dictionary->SetString("process_type",
                        content::GetProcessTypeNameInEnglish(process_type));

  scoped_ptr<base::ListValue> descendants_list(new base::ListValue);
  for (std::vector<ParentChildPairSnapshot>::const_iterator it =
           process_data.descendants.begin();
       it != process_data.descendants.end(); ++it) {
    scoped_ptr<base::DictionaryValue> parent_child(new base::DictionaryValue);
    BirthOnThreadSnapshotToValue(it->parent, "parent", parent_child.get());
    BirthOnThreadSnapshotToValue(it->child, "child", parent_child.get());
    descendants_list->Append(parent_child.release());
  }
  dictionary->Set("descendants", descendants_list.release());
}


bool TaskProfilerDataSerializer::WriteToFile(const base::FilePath& path) {
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);

  scoped_ptr<base::DictionaryValue> root(new DictionaryValue());

  base::ListValue* snapshot_list = new ListValue();
  base::DictionaryValue* shutdown_snapshot = new DictionaryValue();
  base::ListValue* per_process_data = new ListValue();

  root->SetInteger("version", 1);
  root->SetString("userAgent", content::GetUserAgent(GURL()));

  // TODO(ramant): Collect data from other processes, then add that data to the
  // 'per_process_data' array here. Should leverage the TrackingSynchronizer
  // class to implement this.
  ProcessDataSnapshot this_process_data;
  tracked_objects::ThreadData::Snapshot(false, &this_process_data);
  scoped_ptr<base::DictionaryValue> this_process_data_json(
      new base::DictionaryValue);
  TaskProfilerDataSerializer::ToValue(this_process_data,
                                      content::PROCESS_TYPE_BROWSER,
                                      this_process_data_json.get());
  per_process_data->Append(this_process_data_json.release());

  shutdown_snapshot->SetInteger(
      "timestamp",
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
  shutdown_snapshot->Set("data", per_process_data);
  snapshot_list->Append(shutdown_snapshot);
  root->Set("snapshots", snapshot_list);

  serializer.Serialize(*root);
  int data_size = static_cast<int>(output.size());

  return data_size == file_util::WriteFile(path, output.data(), data_size);
}

}  // namespace task_profiler
