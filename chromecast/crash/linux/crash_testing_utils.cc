// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_testing_utils.h"

#include "base/files/file_util.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"

namespace chromecast {
namespace {

const char kDumpsKey[] = "dumps";
const char kRatelimitKey[] = "ratelimit";
const char kRatelimitPeriodStartKey[] = "period_start";
const char kRatelimitPeriodDumpsKey[] = "period_dumps";

// Gets the list of deserialized DumpInfo given a deserialized |lockfile|.
base::ListValue* GetDumpList(base::Value* lockfile) {
  base::DictionaryValue* dict;
  base::ListValue* dump_list;
  if (!lockfile || !lockfile->GetAsDictionary(&dict) ||
      !dict->GetList(kDumpsKey, &dump_list)) {
    return nullptr;
  }

  return dump_list;
}

}  // namespcae

scoped_ptr<DumpInfo> CreateDumpInfo(const std::string& json_string) {
  scoped_ptr<base::Value> value(DeserializeFromJson(json_string));
  return make_scoped_ptr(new DumpInfo(value.get()));
}

bool FetchDumps(const std::string& lockfile_path,
                ScopedVector<DumpInfo>* dumps) {
  if (!dumps) {
    return false;
  }
  dumps->clear();

  base::FilePath path(lockfile_path);
  scoped_ptr<base::Value> contents(DeserializeJsonFromFile(path));
  base::ListValue* dump_list = GetDumpList(contents.get());
  if (!dump_list) {
    return false;
  }

  for (base::Value* elem : *dump_list) {
    scoped_ptr<DumpInfo> dump = make_scoped_ptr(new DumpInfo(elem));
    if (!dump->valid()) {
      return false;
    }
    dumps->push_back(dump.Pass());
  }

  return true;
}

bool ClearDumps(const std::string& lockfile_path) {
  base::FilePath path(lockfile_path);
  scoped_ptr<base::Value> contents(DeserializeJsonFromFile(path));
  base::ListValue* dump_list = GetDumpList(contents.get());
  if (!dump_list) {
    return false;
  }

  dump_list->Clear();

  return SerializeJsonToFile(path, *contents);
}

bool CreateLockFile(const std::string& lockfile_path) {
  scoped_ptr<base::DictionaryValue> output(
      make_scoped_ptr(new base::DictionaryValue()));
  output->Set(kDumpsKey, make_scoped_ptr(new base::ListValue()));

  base::DictionaryValue* ratelimit_fields = new base::DictionaryValue();
  output->Set(kRatelimitKey, make_scoped_ptr(ratelimit_fields));
  ratelimit_fields->SetString(kRatelimitPeriodStartKey, "0");
  ratelimit_fields->SetInteger(kRatelimitPeriodDumpsKey, 0);

  base::FilePath path(lockfile_path);
  const base::Value* val = output.get();
  return SerializeJsonToFile(path, *val);
}

bool AppendLockFile(const std::string& lockfile_path, const DumpInfo& dump) {
  base::FilePath path(lockfile_path);

  scoped_ptr<base::Value> contents(DeserializeJsonFromFile(path));
  if (!contents) {
    CreateLockFile(lockfile_path);
    if (!(contents = DeserializeJsonFromFile(path))) {
      return false;
    }
  }

  base::ListValue* dump_list = GetDumpList(contents.get());
  if (!dump_list) {
    return false;
  }

  dump_list->Append(dump.GetAsValue());

  return SerializeJsonToFile(path, *contents);
}

bool SetRatelimitPeriodStart(const std::string& lockfile_path, time_t start) {
  base::FilePath path(lockfile_path);

  scoped_ptr<base::Value> contents(DeserializeJsonFromFile(path));

  base::DictionaryValue* dict;
  base::DictionaryValue* ratelimit_params;
  if (!contents || !contents->GetAsDictionary(&dict) ||
      !dict->GetDictionary(kRatelimitKey, &ratelimit_params)) {
    return false;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(start));
  std::string period_start_str(buf);
  ratelimit_params->SetString(kRatelimitPeriodStartKey, period_start_str);

  return SerializeJsonToFile(path, *contents);
}

}  // namespace chromecast
