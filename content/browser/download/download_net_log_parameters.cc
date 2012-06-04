// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_net_log_parameters.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "net/base/net_errors.h"

namespace download_net_logs {

namespace {

static const char* download_type_names[] = {
  "NEW_DOWNLOAD",
  "HISTORY_IMPORT",
  "SAVE_PAGE_AS"
};
static const char* download_safety_names[] = {
  "SAFE",
  "DANGEROUS",
  "DANGEROUS_BUT_VALIDATED"
};
static const char* download_danger_names[] = {
  "NOT_DANGEROUS",
  "DANGEROUS_FILE",
  "DANGEROUS_URL",
  "DANGEROUS_CONTENT",
  "MAYBE_DANGEROUS_CONTENT",
  "UNCOMMON_CONTENT"
};

COMPILE_ASSERT(ARRAYSIZE_UNSAFE(download_type_names) == SRC_SAVE_PAGE_AS + 1,
               download_type_enum_has_changed);
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(download_safety_names) ==
                  content::DownloadItem::DANGEROUS_BUT_VALIDATED + 1,
               downloaditem_safety_state_enum_has_changed);
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(download_danger_names) ==
                  content::DOWNLOAD_DANGER_TYPE_MAX,
               download_danger_enum_has_changed);

}  // namespace

ItemActivatedParameters::ItemActivatedParameters(
    DownloadType download_type,
    int64 id,
    const std::string& original_url,
    const std::string& final_url,
    const std::string& file_name,
    content::DownloadDangerType danger_type,
    content::DownloadItem::SafetyState safety_state,
    int64 start_offset)
        : type_(download_type),
          id_(id),
          original_url_(original_url),
          final_url_(final_url),
          file_name_(file_name),
          danger_type_(danger_type),
          safety_state_(safety_state),
          start_offset_(start_offset) {
}

Value* ItemActivatedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("type", download_type_names[type_]);
  dict->SetString("id", base::Int64ToString(id_));
  dict->SetString("original_url", original_url_);
  dict->SetString("final_url", final_url_);
  dict->SetString("file_name", file_name_);
  dict->SetString("danger_type", download_danger_names[danger_type_]);
  dict->SetString("safety_state", download_safety_names[safety_state_]);
  dict->SetString("start_offset", base::Int64ToString(start_offset_));

  return dict;
}

ItemActivatedParameters::~ItemActivatedParameters() {}

ItemCheckedParameters::ItemCheckedParameters(
    content::DownloadDangerType danger_type,
    content::DownloadItem::SafetyState safety_state)
        : danger_type_(danger_type), safety_state_(safety_state) {
}

Value* ItemCheckedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("danger_type", download_danger_names[danger_type_]);
  dict->SetString("safety_state", download_safety_names[safety_state_]);

  return dict;
}

ItemCheckedParameters::~ItemCheckedParameters() {}

ItemInHistoryParameters::ItemInHistoryParameters(int64 handle)
    : db_handle_(handle) {
}

Value* ItemInHistoryParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("db_handle", base::Int64ToString(db_handle_));

  return dict;
}

ItemInHistoryParameters::~ItemInHistoryParameters() {}

ItemUpdatedParameters::ItemUpdatedParameters(int64 bytes_so_far)
    : bytes_so_far_(bytes_so_far) {
}

Value* ItemUpdatedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far_));

  return dict;
}

ItemUpdatedParameters::~ItemUpdatedParameters() {}

ItemRenamedParameters::ItemRenamedParameters(
    const std::string& old_filename, const std::string& new_filename)
        : old_filename_(old_filename), new_filename_(new_filename) {
}

Value* ItemRenamedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("old_filename", old_filename_);
  dict->SetString("new_filename", new_filename_);

  return dict;
}

ItemRenamedParameters::~ItemRenamedParameters() {}

ItemInterruptedParameters::ItemInterruptedParameters(
    content::DownloadInterruptReason reason,
    int64 bytes_so_far,
    const std::string& hash_state)
        : reason_(reason),
          bytes_so_far_(bytes_so_far),
          hash_state_(hash_state) {
}

Value* ItemInterruptedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("interrupt_reason", InterruptReasonDebugString(reason_));
  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far_));
  dict->SetString("hash_state",
                  base::HexEncode(hash_state_.data(), hash_state_.size()));

  return dict;
}

ItemInterruptedParameters::~ItemInterruptedParameters() {}

ItemFinishedParameters::ItemFinishedParameters(
    int64 bytes_so_far,
    const std::string& final_hash)
        : bytes_so_far_(bytes_so_far),
          final_hash_(final_hash) {
}

Value* ItemFinishedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far_));
  dict->SetString("final_hash",
                  base::HexEncode(final_hash_.data(), final_hash_.size()));

  return dict;
}

ItemFinishedParameters::~ItemFinishedParameters() {}

ItemCanceledParameters::ItemCanceledParameters(
    int64 bytes_so_far,
    const std::string& hash_state)
        : bytes_so_far_(bytes_so_far),
          hash_state_(hash_state) {
}

Value* ItemCanceledParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far_));
  dict->SetString("hash_state",
                  base::HexEncode(hash_state_.data(), hash_state_.size()));

  return dict;
}

ItemCanceledParameters::~ItemCanceledParameters() {}

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

FileOpenedParameters::~FileOpenedParameters() {}

FileStreamDrainedParameters::FileStreamDrainedParameters(
    size_t stream_size, size_t num_buffers)
    : stream_size_(stream_size), num_buffers_(num_buffers) {
}

Value* FileStreamDrainedParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetInteger("stream_size", static_cast<int>(stream_size_));
  dict->SetInteger("num_buffers", static_cast<int>(num_buffers_));

  return dict;
}

FileStreamDrainedParameters::~FileStreamDrainedParameters() { }

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

FileRenamedParameters::~FileRenamedParameters() {}

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

FileErrorParameters::~FileErrorParameters() {}

}  // namespace download_net_logs
