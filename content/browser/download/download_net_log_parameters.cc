// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_net_log_parameters.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/download/interrupt_reasons.h"
#include "net/base/net_errors.h"

namespace download_net_logs {

FileOpenedParameters::FileOpenedParameters(const std::string& file_name,
                                           int64 start_offset)
    : file_name_(file_name), start_offset_(start_offset) {
}

Value* FileOpenedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("file_name", file_name_);
  dict->SetString("start_offset", base::Int64ToString(start_offset_));

  return dict;
}

FileRenamedParameters::FileRenamedParameters(
    const std::string& old_filename, const std::string& new_filename)
        : old_filename_(old_filename), new_filename_(new_filename) {
}

Value* FileRenamedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("old_filename", old_filename_);
  dict->SetString("new_filename", new_filename_);

  return dict;
}

FileErrorParameters::FileErrorParameters(const std::string& operation,
                                         net::Error net_error)
    : operation_(operation), net_error_(net_error) {
}

Value* FileErrorParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("operation", operation_);
  dict->SetInteger("net_error", net_error_);

  return dict;
}

}  // namespace download_net_logs
