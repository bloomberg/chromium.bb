// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"

namespace task_profiler {

bool TaskProfilerDataSerializer::WriteToFile(const FilePath &path) {
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
  // 'per_process_data' array here.
  base::DictionaryValue* this_process_data =
      tracked_objects::ThreadData::ToValue(false);
  per_process_data->Append(this_process_data);

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
