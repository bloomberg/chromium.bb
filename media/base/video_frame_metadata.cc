// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_metadata.h"

#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace media {

namespace {

// Map enum key to internal std::string key used by base::DictionaryValue.
inline std::string ToInternalKey(VideoFrameMetadata::Key key) {
  DCHECK_LT(key, VideoFrameMetadata::NUM_KEYS);
  return base::NumberToString(static_cast<int>(key));
}

}  // namespace

VideoFrameMetadata::VideoFrameMetadata() = default;

VideoFrameMetadata::~VideoFrameMetadata() = default;

bool VideoFrameMetadata::HasKey(Key key) const {
  return dictionary_.HasKey(ToInternalKey(key));
}

void VideoFrameMetadata::SetBoolean(Key key, bool value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetInteger(Key key, int value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetDouble(Key key, double value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetRotation(Key key, VideoRotation value) {
  DCHECK_EQ(ROTATION, key);
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetString(Key key, const std::string& value) {
  dictionary_.SetWithoutPathExpansion(
      ToInternalKey(key),
      // Using BinaryValue since we don't want the |value| interpreted as having
      // any particular character encoding (e.g., UTF-8) by
      // base::DictionaryValue.
      base::Value::CreateWithCopiedBuffer(value.data(), value.size()));
}

namespace {
template<class TimeType>
void SetTimeValue(VideoFrameMetadata::Key key,
                  const TimeType& value,
                  base::DictionaryValue* dictionary) {
  const int64_t internal_value = value.ToInternalValue();
  dictionary->SetWithoutPathExpansion(
      ToInternalKey(key), base::Value::CreateWithCopiedBuffer(
                              reinterpret_cast<const char*>(&internal_value),
                              sizeof(internal_value)));
}
}  // namespace

void VideoFrameMetadata::SetTimeDelta(Key key, const base::TimeDelta& value) {
  SetTimeValue(key, value, &dictionary_);
}

void VideoFrameMetadata::SetTimeTicks(Key key, const base::TimeTicks& value) {
  SetTimeValue(key, value, &dictionary_);
}

void VideoFrameMetadata::SetValue(Key key, std::unique_ptr<base::Value> value) {
  dictionary_.SetWithoutPathExpansion(ToInternalKey(key), std::move(value));
}

bool VideoFrameMetadata::GetBoolean(Key key, bool* value) const {
  DCHECK(value);
  return dictionary_.GetBooleanWithoutPathExpansion(ToInternalKey(key), value);
}

bool VideoFrameMetadata::GetInteger(Key key, int* value) const {
  DCHECK(value);
  return dictionary_.GetIntegerWithoutPathExpansion(ToInternalKey(key), value);
}

bool VideoFrameMetadata::GetDouble(Key key, double* value) const {
  DCHECK(value);
  return dictionary_.GetDoubleWithoutPathExpansion(ToInternalKey(key), value);
}

bool VideoFrameMetadata::GetRotation(Key key, VideoRotation* value) const {
  DCHECK_EQ(ROTATION, key);
  DCHECK(value);
  int int_value;
  const bool rv = dictionary_.GetIntegerWithoutPathExpansion(ToInternalKey(key),
                                                             &int_value);
  if (rv)
    *value = static_cast<VideoRotation>(int_value);
  return rv;
}

bool VideoFrameMetadata::GetString(Key key, std::string* value) const {
  DCHECK(value);
  const base::Value* const binary_value = GetBinaryValue(key);
  if (binary_value)
    value->assign(binary_value->GetBlob().begin(),
                  binary_value->GetBlob().end());
  return !!binary_value;
}

namespace {
template <class TimeType>
bool ToTimeValue(const base::Value& binary_value, TimeType* value) {
  DCHECK(value);
  int64_t internal_value;
  if (binary_value.GetBlob().size() != sizeof(internal_value))
    return false;
  memcpy(&internal_value, binary_value.GetBlob().data(),
         sizeof(internal_value));
  *value = TimeType::FromInternalValue(internal_value);
  return true;
}
}  // namespace

bool VideoFrameMetadata::GetTimeDelta(Key key, base::TimeDelta* value) const {
  const base::Value* const binary_value = GetBinaryValue(key);
  return binary_value && ToTimeValue(*binary_value, value);
}

bool VideoFrameMetadata::GetTimeTicks(Key key, base::TimeTicks* value) const {
  const base::Value* const binary_value = GetBinaryValue(key);
  return binary_value && ToTimeValue(*binary_value, value);
}

const base::Value* VideoFrameMetadata::GetValue(Key key) const {
  return dictionary_.FindKey(ToInternalKey(key));
}

bool VideoFrameMetadata::IsTrue(Key key) const {
  bool value = false;
  return GetBoolean(key, &value) && value;
}

std::unique_ptr<base::DictionaryValue> VideoFrameMetadata::CopyInternalValues()
    const {
  return dictionary_.CreateDeepCopy();
}

void VideoFrameMetadata::MergeInternalValuesFrom(const base::Value& in) {
  const base::DictionaryValue* dict;
  if (!in.GetAsDictionary(&dict)) {
    NOTREACHED();
    return;
  }
  dictionary_.MergeDictionary(dict);
}

void VideoFrameMetadata::MergeMetadataFrom(
    const VideoFrameMetadata* metadata_source) {
  dictionary_.MergeDictionary(&metadata_source->dictionary_);
}

const base::Value* VideoFrameMetadata::GetBinaryValue(Key key) const {
  const base::Value* internal_value = dictionary_.FindKey(ToInternalKey(key));
  if (internal_value && (internal_value->type() == base::Value::Type::BINARY))
    return internal_value;
  return nullptr;
}

}  // namespace media
