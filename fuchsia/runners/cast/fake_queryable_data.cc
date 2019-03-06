// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/fake_queryable_data.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"

FakeQueryableData::FakeQueryableData() = default;

FakeQueryableData::~FakeQueryableData() = default;

void FakeQueryableData::Add(base::StringPiece key, const base::Value& value) {
  std::string value_json;
  CHECK(base::JSONWriter::Write(value, &value_json));
  chromium::cast::QueryableDataEntry cur_entry_converted = {key.as_string(),
                                                            value_json};
  entries_[key.as_string()] = cur_entry_converted;
}

void FakeQueryableData::GetChangedEntries(GetChangedEntriesCallback callback) {
  std::vector<chromium::cast::QueryableDataEntry> output;
  for (const auto& e : entries_) {
    output.push_back(e.second);
  }
  callback(output);
}
