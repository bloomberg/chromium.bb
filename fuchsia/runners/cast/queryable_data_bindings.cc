// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/queryable_data_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/common/javascript_injection.h"

namespace {
constexpr char kBindingsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/queryable_data_bindings.js");
}  // namespace

QueryableDataBindings::QueryableDataBindings(
    chromium::web::Frame* frame,
    fidl::InterfaceHandle<chromium::cast::QueryableData> service)
    : frame_(frame), service_(service.Bind()) {
  DCHECK(frame_);

  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  InjectJavaScriptFileIntoFrame(assets_path.AppendASCII(kBindingsPath), frame_);

  service_->GetChangedEntries(
      fit::bind_member(this, &QueryableDataBindings::OnEntriesReceived));
}

QueryableDataBindings::~QueryableDataBindings() = default;

void QueryableDataBindings::OnEntriesReceived(
    std::vector<chromium::cast::QueryableDataEntry> values) {
  // Prepare values for serialization as a JavaScript object literal.
  base::DictionaryValue output;
  for (const chromium::cast::QueryableDataEntry& current : values) {
    base::Optional<base::Value> value_parsed =
        base::JSONReader::Read(current.json_value);
    if (!value_parsed) {
      LOG(WARNING) << "Couldn't parse QueryableData item: " << current.key;
      continue;
    }

    output.SetKey(current.key, std::move(*value_parsed));
  }

  std::string output_json;
  if (!base::JSONWriter::Write(output, &output_json)) {
    LOG(ERROR)
        << "An unexpected error occurred while serializing QueryableData.";
    return;
  }

  // Generate the JavaScript snippet that will register the values with the
  // bindings library.
  std::string initialize_values_js = base::StringPrintf(
      "cast.__platform__.__queryPlatformValueStore__.initialize(%s);",
      output_json.c_str());
  output_json.clear();
  output_json.shrink_to_fit();

  fuchsia::mem::Buffer initialize_values_js_buffer =
      cr_fuchsia::MemBufferFromString(initialize_values_js);
  initialize_values_js.clear();
  initialize_values_js.shrink_to_fit();

  InjectJavaScriptBufferIntoFrame(std::move(initialize_values_js_buffer),
                                  frame_);

  // TODO(crbug.com/929291): Handle change events by calling and waiting on
  // GetChangedEntries() here.
}
