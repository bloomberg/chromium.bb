// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/web_resource_unpacker.h"

#include "base/json/json_reader.h"
#include "base/values.h"

const char* WebResourceUnpacker::kInvalidDataTypeError =
    "Data from web resource server is missing or not valid JSON.";

const char* WebResourceUnpacker::kUnexpectedJSONFormatError =
    "Data from web resource server does not have expected format.";

WebResourceUnpacker::WebResourceUnpacker(const std::string &resource_data)
    : resource_data_(resource_data) {
}

WebResourceUnpacker::~WebResourceUnpacker() {
}

// TODO(mrc): Right now, this reads JSON data from the experimental popgadget
// server.  Change so the format is based on a template, once we have
// decided on final server format.
bool WebResourceUnpacker::Run() {
  scoped_ptr<base::Value> value;
  if (!resource_data_.empty()) {
    value.reset(base::JSONReader::Read(resource_data_));
    if (!value.get()) {
      // Page information not properly read, or corrupted.
      error_message_ = kInvalidDataTypeError;
      return false;
    }
    if (!value->IsType(base::Value::TYPE_DICTIONARY)) {
      error_message_ = kUnexpectedJSONFormatError;
      return false;
    }
    parsed_json_.reset(static_cast<base::DictionaryValue*>(value.release()));
    return true;
  }
  error_message_ = kInvalidDataTypeError;
  return false;
}

