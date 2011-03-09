// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_format.h"

namespace media {

const char MediaFormat::kURL[]              = "URL";
const char MediaFormat::kSurfaceFormat[]    = "SurfaceFormat";
const char MediaFormat::kSurfaceType[]      = "SurfaceType";
const char MediaFormat::kSampleRate[]       = "SampleRate";
const char MediaFormat::kSampleBits[]       = "SampleBits";
const char MediaFormat::kChannels[]         = "Channels";
const char MediaFormat::kWidth[]            = "Width";
const char MediaFormat::kHeight[]           = "Height";

MediaFormat::MediaFormat() {}

MediaFormat::~MediaFormat() {
  Clear();
}

bool MediaFormat::Contains(const std::string& key) const {
  return value_map_.find(key) != value_map_.end();
}

void MediaFormat::Clear() {
  for (ValueMap::iterator iter(value_map_.begin());
       iter != value_map_.end(); ++iter)
    delete iter->second;
  value_map_.clear();
}

void MediaFormat::SetAsBoolean(const std::string& key, bool in_value) {
  ReleaseValue(key);
  value_map_[key] = Value::CreateBooleanValue(in_value);
}

void MediaFormat::SetAsInteger(const std::string& key, int in_value) {
  ReleaseValue(key);
  value_map_[key] = Value::CreateIntegerValue(in_value);
}

void MediaFormat::SetAsReal(const std::string& key, double in_value) {
  ReleaseValue(key);
  value_map_[key] = Value::CreateDoubleValue(in_value);
}

void MediaFormat::SetAsString(const std::string& key,
                              const std::string& in_value) {
  ReleaseValue(key);
  value_map_[key] = Value::CreateStringValue(in_value);
}

bool MediaFormat::GetAsBoolean(const std::string& key, bool* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsBoolean(out_value);
}

bool MediaFormat::GetAsInteger(const std::string& key, int* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsInteger(out_value);
}

bool MediaFormat::GetAsReal(const std::string& key, double* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsDouble(out_value);
}

bool MediaFormat::GetAsString(const std::string& key,
                              std::string* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsString(out_value);
}

Value* MediaFormat::GetValue(const std::string& key) const {
  ValueMap::const_iterator value_iter = value_map_.find(key);
  return (value_iter == value_map_.end()) ? NULL : value_iter->second;
}

void MediaFormat::ReleaseValue(const std::string& key) {
  ValueMap::iterator vm = value_map_.find(key);
  if (vm != value_map_.end()) {
    delete vm->second;
  }
}

}  // namespace media
